/*
 * sendPacket.c
 *
 *  Created on: Apr 21, 2017
 *      Author: root
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

//network
#include <net/ethernet.h>
#include <stdint.h>

#include "calm_debug.h"
#include "tools.h"
#include "xpmon_be.h"

#ifdef DEBUG_VERBOSE /* Enable both normal and verbose logging */
#define log_verbose(args...)    D(args)
#else
#define log_verbose(x...)
#endif

#define MIN_PKTSIZE 64
#define MAX_PKTSIZE (32*1024)

#define MAX_THREADS 12

#define PAGE_SIZE (4096)
#define BUFFER_SIZE (PAGE_SIZE * 2048 * 4)	//8k pages


#define MAC_FMT  "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_BUF_LEN 18 //
#define DECLARE_MAC_BUF(var)  char var[MAC_BUF_LEN] //

/*
 * macString:	mac address like "ab:cd:ef:11:22:33"
 * addr 	: 	an array of u_int8_t[6]
 */
static void macString2Array(char * macString, u_int8_t * addr){
	sscanf(macString, MAC_FMT,addr,addr+1,addr+2,addr+3,addr+4,addr+5);
}

static void usage(void){
	const char * cmd = "sendPacket";
	fprintf(stderr,
			"Usage:\n"
			"%s arguments\n"
			"\t-S size		batched packet size sends every time\n"
			"\t-s size		per packet size in the batched packet\n"
			"",
			cmd);

	exit(0);
}

//global variables
int thread_running[MAX_THREADS]; //0:packetReceive 1:packet send no interval 2:packetc send with interval
int dma_channel0_exit;
int dma_channel1_exit;
int dma_channel2_exit;
int dma_channel3_exit;

#ifdef MEMORY_FEEDBACK
MemorySync TxDoneSync0={0,PTHREAD_MUTEX_INITIALIZER};
MemorySync TxDoneSync1={0,PTHREAD_MUTEX_INITIALIZER};
MemorySync TxDoneSync2={0,PTHREAD_MUTEX_INITIALIZER};
MemorySync TxDoneSync3={0,PTHREAD_MUTEX_INITIALIZER};
#endif

static int batchedPacketSize;
static int perPacketSize;

void * receive_data(TestCmd * test);
void * sendPacket_once(TestCmd * test);
void * sendPacket_interval(TestCmd * test);
void * sendPacket(TestCmd * test);

int setMacHeader(unsigned char * buffer,
		char * dmac_string,
		char * smac_string,
		u_int16_t type)
{
	u_int8_t ether_dmac[ETH_ALEN], ether_smac[ETH_ALEN];
	macString2Array(dmac_string,ether_dmac);
	macString2Array(smac_string,ether_smac);

	struct ether_header * curHeader = (struct ether_header *)buffer;
	memcpy(&(curHeader->ether_dhost), ether_dmac, ETH_ALEN);
	memcpy(&(curHeader->ether_shost), ether_smac, ETH_ALEN);
	curHeader->ether_type = type;
	return 0;
}

static void FormatBuffer(unsigned char * buf,int bufferSize,int chunksize,int pktSize)
{
	int i,j=0;
	unsigned short TxSeqNo=0;
	/* Apply data pattern in the buffer */

	while( j  <  bufferSize)
	{
		for(i = 0; i < chunksize; i = i+2)
		{
			if(i==0)
				*(unsigned short *)(buf + j + i) = pktSize;
			else
				*(unsigned short *)(buf + j + i) = TxSeqNo;

		}
		j +=i;
		TxSeqNo++;
		if(TxSeqNo >= TX_CONFIG_SEQNO)
			TxSeqNo=0;
	}
}

static void FormatBuffer1(unsigned char * buf,int bufferSize,int chunksize)
{
	int i,j=0;
	/* Apply data pattern in the buffer */
	unsigned short TxSeqNo=0;
	chunksize += (chunksize%2);
	while( j + chunksize  <  bufferSize)
	{
		for(i = 0; i < chunksize; i = i+2)
		{
			if(i==0)
				*(unsigned short *)(buf + j + i) = bufferSize;
			else
				*(unsigned short *)(buf + j + i) = TxSeqNo;

		}
		j +=i;
		TxSeqNo++;
		if(TxSeqNo >= TX_CONFIG_SEQNO)
			TxSeqNo=0;

	}
	while(j<bufferSize){
		*(unsigned char *)(buf + j) = 0xFF;
		j++;
	}
}

static void FormatBatchedBuffer(unsigned char * buf, int batchedSize, int perSize){
	int i;
	int maxNo = 512;

	for(i = 0; i < batchedSize; i++ ){
		*(buf + i) = i % maxNo;
	}

	for(i = 0; i< batchedSize / perSize ; i++){
		setMacHeader(buf + i * perSize,
					"aa:bb:cc:dd:ee:ff",
					"87:87:87:87:87:87",
					ETHERTYPE_IP);
	}

}

void printMenu()
{
	printf("************************************\n"
			"====================================\n\n\n");
	printf("====================================\n"
			"************************************\n");
	printf("\t1.show status\n"
			"\t2.start receive thread\n"
			"\t3.send one packet\n"
			"\t4.start sendpacket thread no interval\n"
			"\t5.start sendpacket thread at interval 100ms\n"
			"\t9.stop test\n"
			"\t0.exit		(enter your option):\n");
}

void showStatus()
{
	if(thread_running[0]){
		printf("receive thread is: running\n");
	}else{
		printf("receive thread is: not running\n");
	}

	if(thread_running[1]){
		printf("sendpacket no interval thread is: running\n");
	}else{
		printf("sendpacket no interval thread is: not running\n");
	}

	if(thread_running[2]){
		printf("sendpacket at interval thread is: running\n");
	}else{
		printf("sendpacket at interval thread is: not running\n");
	}
}

int readReservedMM(){
	void * readBuffer;
	int readfd;
	int size = 2048;

	readfd = open("/dev/xraw_data0", O_RDWR);
	if(readfd < 0){
		printf("Can't open file \n");
		exit(-1);
	}
	if((readBuffer = mmap(NULL, PAGE_SIZE * size, PROT_READ, MAP_SHARED, readfd, 0)) == MAP_FAILED)
	{
		printf("mmap failed.\n");
		exit(-1);
	}

//	hexPrint((char *) readBuffer, 200);
	int i = 0;
	for(i =0; i< 200; i++){
		printf("%c",((char *)readBuffer)[i] );
	}


	if(munmap(readBuffer, PAGE_SIZE * size) != 0)
	{
		printf("munmap failed\n");
	}
	close(readfd);
	return 0;

}

int test(){
	unsigned char * buf = (unsigned char *)malloc(batchedPacketSize);
	FormatBatchedBuffer(buf,batchedPacketSize, perPacketSize);
	batchedPacketPrint(buf,batchedPacketSize,perPacketSize);
	return 0;
}


int main(int argc, char ** argv){
	int control, i, ret;
	int batchedSize = 0, perSize = 0;
	TestCmd testCmd;

	int ch;

	while( (ch = getopt(argc, argv,"s:S:")) != -1){
		switch(ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;
		case 'S':
			i = atoi(optarg);
			if(i < 64 || i > 9999){
				D("invalid argument of '-S'! batched packet size is %d, it should between 64 and 9999",i);
				break;
			}
			batchedSize = i;
			break;

		case 's':
			i = atoi(optarg);
			if(i < 64 || i > 9999){
				D("invalid argument of '-s'! per packet size is %d, it should between 64 and 9999",i);
				break;
			}
			perSize = i;
			break;
		}
	}

	if(batchedSize == 0){
		D("missing argument \"-S batchedSize\"");
		usage();
	}else{
		batchedPacketSize = batchedSize;
	}

	if(perSize == 0){
		D("missing argument \"-s per_packet_size\"");
		usage();
	}else{
		if((batchedSize < perSize) || ((batchedSize % perSize) != 0)){
			if(batchedSize < perSize)
				D("per paket size (%d) is smaller than batched packet size (%d)!", i, batchedSize);
			if((batchedSize % perSize) != 0)
				D("batched packet size (%d) should be integral multiple of per pakcet size (%d)",batchedSize ,i);
			exit(0);
		}
		perPacketSize = perSize;
	}

	int tmode = ENABLE_LOOPBACK;
//	int tmode = ENABLE_PKTCHK;
//	int tmode = ENABLE_PKTGEN;
//	int tmode = ENABLE_PKTCHK|ENABLE_PKTGEN;

	testCmd.Engine = 0;
	testCmd.TestMode = tmode;
	testCmd.MaxPktSize = batchedSize;
	testCmd.MinPktSize = perSize;

	dma_channel0_exit = 0;		//thread 0 is not exit
	for(i = 0; i< MAX_THREADS; i++){
		thread_running[i] = 0;
	}

	control = 1;
	while(control != 0)
	{
		printMenu();
		printf("************************************\n");
		scanf("%d",&control);getchar();
		printf("************************************\n");
		switch(control){
		case 1:
			showStatus();
			break;
		case 2:
		{
			if(testCmd.Engine == 0){
				if(thread_running[0] == 0)
					ret = xlnx_thread_create(&receive_data, &testCmd);
				else{
					printf("Exiting at %d %s()\n",__LINE__,__FUNCTION__);
					exit(-1);
				}

				if(ret){
					printf("ERROR; return code from pthread_create() is %d\n", ret);
					exit(-1);
				}
			}
			sleep(1);
			break;
		}
		case 3:
		{
			ret = xlnx_thread_create(&sendPacket_once, &testCmd);
			if (ret){
				printf("ERROR; return code from pthread_create() is %d\n", ret);
				exit(-1);
			}
			sleep(1);
			break;
		}
		case 4:
			if(testCmd.Engine == 0 ){
				if(thread_running[1] == 0)
					ret = xlnx_thread_create(&sendPacket, &testCmd);
				else{
					printf("Exiting at %d %s()\n",__LINE__,__FUNCTION__);
					exit(-1);
				}

				if (ret){
					printf("ERROR; return code from pthread_create() is %d\n", ret);
					exit(-1);
				}
			}
			sleep(1);
			break;
		case 5:
			if(testCmd.Engine == 0 ){
				if(thread_running[2] == 0)
					ret = xlnx_thread_create(&sendPacket_interval, &testCmd);
				else{
					printf("Exiting at %d %s()\n",__LINE__,__FUNCTION__);
					exit(-1);
				}

				if (ret){
					printf("ERROR; return code from pthread_create() is %d\n", ret);
					exit(-1);
				}
			}
			sleep(1);
			break;
		case 8:
			test();
			break;
		case 9:
			dma_channel0_exit = 1;
			printf("stop all the test\n");
			sleep(2);
			break;
		case 0:
		default:
			break;
		}

	}

	printf("exit!\n");
	printf("************************************\n"
			"====================================\n\n\n");
	return 0;
}

void * receive_data(TestCmd * test){
	int readfd = 0, ret_val;
	FreeInfo userInfo;
	BufferInfo packetInfo;
	int i,startNo,endNo,pageNum;
	unsigned int completed = 0, freePages = 0;
	unsigned int packetSize;
	RxPacketInfo rxInfo;

	MemorySync *TxDoneSync;
	char * readBuffer;	//mmap RxBuf to readBuffer
	int retry = 0;

	int engine = test->Engine;

#ifdef WRITE_TO_FILE
	//read RX buffer data, and store them to local file
	FILE * fp_read;
	fp_read = fopen("RX_read_data.txt","w+");
	if(fp_read== NULL){
		D("open file tx_write_log.txt failed!");
	}
#endif


	if(engine == 0){
		readfd= open("/dev/xraw_data0",O_RDWR);
		if(readfd < 0){
			D("Can't open file \"/dev/xraw_data0\"");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync0;
#endif
		thread_running[0] = 1;
	}else if(engine == 1){
		readfd= open("/dev/xraw_data1",O_RDWR);
		if(readfd < 0){
			D("Can't open file \"/dev/xraw_data1\"");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync1;
#endif
		thread_running[3] = 1;
	}else if(engine == 2){
		readfd= open("/dev/xraw_data2",O_RDWR);
		if(readfd < 0){
			D("Can't open file \"/dev/xraw_data2\"");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync2;
#endif
		thread_running[6] = 1;
	}else if(engine == 3){
		readfd= open("/dev/xraw_data3",O_RDWR);
		if(readfd < 0){
			D("Can't open file \"/dev/xraw_data3\"");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync3;
#endif
		thread_running[9] = 1;
	}

	/** using ioctl to read the driver info to get the RxBufs size
	 * 	rxInfo.bufferNum will be replaced by RxBufs.TotoalNum
	 * 	then using it set the mmmap size: PAGE_SIZE * rxInfo.bufferNum */
	rxInfo.bufferNum = 0;
	rxInfo.freeNum = 0;
	ret_val=ioctl(readfd,IGET_RX_PACKETINFO, &rxInfo);
	if(ret_val < 0){
		D("receive IOCTL FAILED:%d ",ret_val);
		exit(-1);
	}
	D("call ioctl(), get the RxBufs size is %d",rxInfo.bufferNum);

#ifdef MMap
	if(rxInfo.bufferNum > 0)
	{
		if((readBuffer = mmap(NULL,PAGE_SIZE * rxInfo.bufferNum, PROT_READ ,MAP_SHARED, readfd, 0)) == MAP_FAILED)
		{
			 D("mmap failed.");
			 exit(-1);
		}
		D("mmap(), set memory map done");
	}else{
		D("cannot get the rxbufs number from dirver!");
		exit(-1);
	}
#endif

	unsigned long count = 0;
	D("start loop for stroing RX data");
	while(1)
	{
		//call ioctl to know how many packet rx received
		completed = 0;
		freePages = 0;
		userInfo.expected = MAX_LIST;
		ret_val=ioctl(readfd,IGET_TRN_RXUSRINFO, &userInfo);
		if(ret_val < 0){
			printf("receive IOCTL FAILED\n");
			break;
		}

		if(userInfo.expected > 0)
		{
			log_verbose("return from IGET_TRN_RXUSRINFO, expected is %d",userInfo.expected);
			//if rx get packet, then read the data
			for(i =0; i< userInfo.expected ; i++)
			{
				packetInfo = userInfo.buffList[i];
				startNo = packetInfo.startNo;
				endNo = packetInfo.endNo;
				pageNum = packetInfo.noPages;
				packetSize = packetInfo.buffSize;

				count++;
				log_verbose("receive %d packet >>> startNo:%d, endNo:%d, pageNum:%d, packetSize:%d",count, startNo,endNo,pageNum,packetSize);
//				printf("receive %d packet >>> pageNum:%d, packetSize:%d\n",count, pageNum,packetSize);
				if((count % 10000) == 0)
					D("have received %d packets",count);
#ifdef DEBUG_VERBOSE
				batchedPacketPrint(readBuffer + startNo * PAGE_SIZE, packetSize ,perPacketSize );
#endif

#ifdef WRITE_TO_FILE
				/** write the buffer data to local file*/
				fprintf(fp_read, "%d read data:\n",count);
				if(count <= 9999){
					fhexPrint(fp_read, readBuffer + startNo * PAGE_SIZE, packetSize);
				}
#endif

				completed += packetSize;
				freePages += pageNum;
			}
			//free the RxBuffers in which the rx data read from it and store to local file
			rxInfo.freeNum = freePages;
			ret_val=ioctl(readfd,IGET_RX_PACKETINFO, &rxInfo);
			if(ret_val < 0){
				D("receive IOCTL FAILED:%d ",ret_val);
				exit(-1);
			}

#ifdef MEMORY_FEEDBACK
			if (0 != FreeAvailable(TxDoneSync, PAGE_SIZE * freePages))
			{
				perror("Bad Pointer TxDoneSync: MemorySync");
			}
#endif
		}
		if(engine == 0){
			if( (dma_channel0_exit == 1) && (userInfo.expected ==0))
			{
//				printf("***<thread: receive_data>***  get stop signal,ready to stop, retry is %d\n",retry);
				if(retry >= 15){
					thread_running[0] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}else if(engine == 1){
			if( (dma_channel1_exit == 1) && (userInfo.expected ==0))
			{
				if(retry >= 15){
					thread_running[2] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}else if(engine == 2){
			if( (dma_channel2_exit == 1) && (userInfo.expected ==0))
			{
				if(retry >= 15){
					thread_running[4] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}else if(engine == 3){
			if( (dma_channel3_exit == 1) && (userInfo.expected ==0))
			{
				if(retry >= 15){
					thread_running[6] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}
	}

ERROR:
	D("thread receive_data stopped");
#ifdef MMap
	if(munmap(readBuffer,PAGE_SIZE * rxInfo.bufferNum) !=0 ){
		D("munmap failed");
	}
#endif

#ifdef WRITE_TO_FILE
	fclose(fp_read);
#endif
	close(readfd);
	pthread_exit(NULL);
}

void * sendPacket_once(TestCmd * test){
	unsigned char * buffer;
	int file_desc,ret;
	int batchedSize = test->MaxPktSize;
	int perSize = test->MinPktSize;
	int engine = test->Engine;

	buffer = (char *) malloc(batchedSize);

	if(!buffer)
	{
		printf("Unable to allocate memory \n");
		exit(-1);
	}
	if(engine == 0)
	{
		file_desc= open("/dev/xraw_data0",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}
	}
	FormatBatchedBuffer(buffer,batchedSize, perSize);

	printf("After format, packet is:\n");
	batchedPacketPrint(buffer, batchedSize, perSize);


	ret=write(file_desc,buffer,batchedSize);
	if(ret < batchedSize)
	{
		printf("write packet smaller than expected!\n");
	}else{
		printf("write packet success!\n");
	}
	//clear works
	close(file_desc);
	free(buffer);

	pthread_exit(NULL);
}

void * sendPacket(TestCmd * test){
	unsigned char * buffer;
	unsigned long bufferLen;
	int file_desc, ret;
	int packetSize = test->MaxPktSize;
	int engine = test->Engine;
	unsigned long count = 0;

	MemorySync * TxDoneSync = NULL;
	int chunksize=0;
	int PacketSent=0;

	if(engine == 0 && thread_running[2])
	{
		D("thread sendPacket_interval is running, cannot start 2 send packet thread!");
		thread_running[1] = 0;
		D("terminate sendPacket thread for engine %d",engine);
		pthread_exit(NULL);
	}else if(engine == 1 && thread_running[5])
	{
		D("thread sendPacket_interval is running, cannot start 2 send packet thread!");
		thread_running[4] = 0;
		D("terminate sendPacket thread for engine %d",engine);
		pthread_exit(NULL);
	}if(engine == 2 && thread_running[8])
	{
		D("thread sendPacket_interval is running, cannot start 2 send packet thread!");
		thread_running[7] = 0;
		D("terminate sendPacket thread for engine %d",engine);
		pthread_exit(NULL);
	}if(engine == 3 && thread_running[11])
	{
		D("thread sendPacket_interval is running, cannot start 2 send packet thread!");
		thread_running[10] = 0;
		D("terminate sendPacket thread for engine %d",engine);
		pthread_exit(NULL);
	}


	if(packetSize % 4){
		chunksize = packetSize + (4 - (packetSize % 4));
	}else{
		chunksize = packetSize;
	}

	bufferLen = BUFFER_SIZE - (BUFFER_SIZE % (chunksize * 512));
	buffer = (char *) valloc(bufferLen);
	if(!buffer)
	{
		printf("Unable to allocate memory \n");
		exit(-1);
	}

	if(engine == 0)
	{
		file_desc= open("/dev/xraw_data0",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}

		thread_running[1] = 1;
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync0;
#endif
	}else if(engine == 1){
		file_desc= open("/dev/xraw_data1",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}

		thread_running[4] = 1;
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync1;
#endif
	}else if(engine == 2){
		file_desc= open("/dev/xraw_data2",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}

		thread_running[7] = 1;
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync2;
#endif
	}else if(engine == 3){
		file_desc= open("/dev/xraw_data3",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}

		thread_running[10] = 1;
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync3;
#endif
	}

//initialize the available memory with the total memory.
#ifdef MEMORY_FEEDBACK
	if (0 != initMemorySync(TxDoneSync, PAGE_SIZE * 2048))
	{
		perror("Bad Pointer TxDoneSync: MemorySync");
	}
#endif
	FormatBuffer(buffer,bufferLen,chunksize,packetSize);

	D("thread sendPacket start loop for sending packets");
	while(1)
	{
#ifdef MEMORY_FEEDBACK
		if(0 == ReserveAvailable(TxDoneSync, PAGE_SIZE))
		{
			if(PacketSent + chunksize <= bufferLen )
			{
				ret=write(file_desc,buffer+PacketSent,packetSize);
				count ++;
				if(ret < packetSize)
				{
					FreeAvailable(TxDoneSync, PAGE_SIZE);
					//TODO
					D("send packet no.%ld get ret is%d, smaller than expected %d!",count ,ret, packetSize);
				}
				else
				{
					PacketSent = PacketSent + chunksize;
				}

			}
			else
			{
				FreeAvailable(TxDoneSync,PAGE_SIZE);
				PacketSent = 0;
			}

		}
#else
		if(PacketSent + chunksize <= bufferLen )
		{
			ret=write(file_desc,buffer+PacketSent,packetSize);
			count ++;
			if(ret < packetSize)
			{
				D("send packet no.%ld get ret is%d, smaller than expected %d!",count ,ret, packetSize);
			}
			else
			{
				PacketSent = PacketSent + chunksize;
			}

		}
		else
		{
			PacketSent = 0;
		}
#endif
		if(engine == 0)
		{
			if(dma_channel0_exit == 1){
				D("thread sendPacket get stop signal,ready to stop");
				thread_running[1] = 0;
				while(thread_running[0]);
				goto ERROR;
			}
		}else if(engine == 1)
		{
			if(dma_channel1_exit == 1){
				thread_running[4] = 0;
				while(thread_running[3]);
				goto ERROR;
			}
		}else if(engine == 2)
		{
			if(dma_channel2_exit == 1){
				thread_running[7] = 0;
				while(thread_running[6]);
				goto ERROR;
			}
		}else if(engine == 3)
		{
			if(dma_channel3_exit == 1){
				thread_running[10] = 0;
				while(thread_running[9]);
				goto ERROR;
			}
		}

	}

ERROR:
	D("thread sendPacket stopped");
	//clear works
	close(file_desc);
	free(buffer);
	pthread_exit(NULL);

}

void * sendPacket_interval(TestCmd * test){
	char *buffer;
	unsigned long bufferLen;
	int file_desc,ret;
	int packetSize = test->MaxPktSize;
	int engine = test->Engine;
	unsigned long count = 0;

	MemorySync * TxDoneSync = NULL;
	int chunksize=0;
	int PacketSent=0;

	if(engine == 0 && thread_running[1])
	{
		D("thread sendPacket is running, cannot start 2 send packet thread!");
		D("terminate sendPacket_interval thread for engine %d",engine);
		thread_running[2] = 0;
		pthread_exit(NULL);

	}


	if(packetSize % 4){
		chunksize = packetSize + (4 - (packetSize % 4));
	}else{
		chunksize = packetSize;
	}

	bufferLen = BUFFER_SIZE - (BUFFER_SIZE % (chunksize * 512));
	buffer = (char *) valloc(bufferLen);
	if(!buffer)
	{
		D("Unable to allocate memory");
		exit(-1);
	}
	if(engine == 0)
	{
		file_desc= open("/dev/xraw_data0",O_RDWR);
		if(file_desc < 0){
			printf("Can't open file \n");
			free(buffer);
			exit(-1);
		}
		thread_running[2] = 1;

#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync0;
#endif
	}

	FormatBuffer(buffer,bufferLen,chunksize,packetSize);

	while(1)
	{
#ifdef MEMORY_FEEDBACK
		if(0 == ReserveAvailable(TxDoneSync, PAGE_SIZE))
		{
			if(PacketSent + chunksize <= bufferLen )
			{
				ret=write(file_desc,buffer+PacketSent,packetSize);
				count ++;
				if(ret < packetSize)
				{
					FreeAvailable(TxDoneSync, PAGE_SIZE);
					//TODO
					D("send packet no.%ld get ret is%d, smaller than expected %d!",count ,ret, packetSize);
				}
				else
				{
					PacketSent = PacketSent + chunksize;
				}
				usleep(10*1000);
			}
			else
			{
				FreeAvailable(TxDoneSync,PAGE_SIZE);
				PacketSent = 0;
			}
		}
#else
		if(PacketSent + chunksize <= bufferLen )
		{
			ret=write(file_desc,buffer+PacketSent,packetSize);
			count ++;
			if(ret < packetSize)
			{
				D("send packet no.%ld get ret is%d, smaller than expected %d!",count ,ret, packetSize);
			}
			else
			{
				PacketSent = PacketSent + chunksize;
			}
			usleep(10*1000);
		}
		else
		{
			PacketSent = 0;
		}
#endif


		if(engine == 0)
		{
			if(dma_channel0_exit == 1){
				D("thread sendPacket_interval get stop signal,ready to stop");
				thread_running[2] = 0;
				while(thread_running[0]);
				goto ERROR;
			}
		}

	}

ERROR:
	D("thread sendPacket_interval stopped");
	//clear works
	close(file_desc);
	free(buffer);

	pthread_exit(NULL);
}
