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
#define log_verbose(args...)    printf(args)
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

static void macString2Array(char * macString, u_int8_t * addr){
	sscanf(macString, MAC_FMT,addr,addr+1,addr+2,addr+3,addr+4,addr+5);
}


//global variables
int thread_running[MAX_THREADS];
int thread0_exit;
int thread1_exit;
int thread2_exit;
int thread3_exit;

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

static void FormatBuffer2(unsigned char * buf, int bufferSize){
	int i;

	setMacHeader(buf,
			"aa:bb:cc:dd:ee:ff",
			"87:87:87:87:87:87",
			ETHERTYPE_IP);

	i = sizeof(struct ether_header);
	for(; i < bufferSize; i++ ){
		*(buf + i) = i;
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
	unsigned char * buf = (unsigned char *)malloc(100);
	FormatBuffer2(buf,100);
	macPacketPrint(buf,100);
	D("gaobin shi ge sb %d",sizeof(buf));
	return 0;
}


int main(int argc, char ** argv){
	int control, i, ret;
	int psize;
	TestCmd testCmd;

	if (argc != 2)
	{
		printf("error!\n"
				"this exe needs only one arguement, which indicate the packet size.\n"
				"for example: sendPacket 230\n");
		return -1;
	}else{
		psize = atoi(argv[1]);
		if(psize < 64 || psize > 9999){
			printf("error!\n"
					"packet size is %d, it should between 64 and 9999\n",psize);
			return -1;
		}
	}

	int tmode = ENABLE_LOOPBACK;
//	int tmode = ENABLE_PKTCHK;
//	int tmode = ENABLE_PKTGEN;
//	int tmode = ENABLE_PKTCHK|ENABLE_PKTGEN;

	testCmd.Engine = 0;
	testCmd.TestMode = tmode;
	testCmd.MaxPktSize = psize;

	thread0_exit = 0;		//thread 0 is not exit
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
			break;
		}
		case 3:
		{
			ret = xlnx_thread_create(&sendPacket_once, &testCmd);
			if (ret){
				printf("ERROR; return code from pthread_create() is %d\n", ret);
				exit(-1);
			}
			break;
		}
		case 4:
			readReservedMM();
			break;
		case 5:
			break;

		case 8:
			test();
			break;
		case 9:
			thread0_exit = 1;
			printf("stop all the test\n");
			break;
		case 0:
		default:
			break;
		}
		sleep(2);

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
		printf("open file tx_write_log.txt failed!\n");
	}
#endif


	if(engine == 0){
		readfd= open("/dev/xraw_data0",O_RDWR);
		if(readfd < 0){
			printf("Can't open file \n");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync0;
#endif
		thread_running[0] = 1;
	}else if(engine == 1){
		readfd= open("/dev/xraw_data1",O_RDWR);
		if(readfd < 0){
			printf("Can't open file \n");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync1;
#endif
		thread_running[3] = 1;
	}else if(engine == 2){
		readfd= open("/dev/xraw_data2",O_RDWR);
		if(readfd < 0){
			printf("Can't open file \n");
			exit(-1);
		}
#ifdef MEMORY_FEEDBACK
		TxDoneSync = &TxDoneSync2;
#endif
		thread_running[6] = 1;
	}else if(engine == 3){
		readfd= open("/dev/xraw_data3",O_RDWR);
		if(readfd < 0){
			printf("Can't open file \n");
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
		printf("receive IOCTL FAILED:%d\n ",ret_val);
		exit(-1);
	}
	printf("***<thread: receive_data>***  call ioctl(), get the RxBufs size is %d\n",rxInfo.bufferNum);

#ifdef MMap
	if(rxInfo.bufferNum > 0)
	{
		if((readBuffer = mmap(NULL,PAGE_SIZE * rxInfo.bufferNum, PROT_READ ,MAP_SHARED, readfd, 0)) == MAP_FAILED)
		{
			 printf("mmap failed.\n");
			 exit(-1);
		}
		printf("***<thread: receive_data>***  mmap(), set memory map done\n");
	}else{
		printf("cannot get the rxbufs number from dirver\n!");
		exit(-1);
	}
#endif

	unsigned long count = 0;
	printf("***<thread: receive_data>***  start loop for stroing RX data\n");
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
			printf("***<thread: receive_data>***  return from IGET_TRN_RXUSRINFO, expected is %d\n",userInfo.expected);
			//if rx get packet, then read the data
			for(i =0; i< userInfo.expected ; i++)
			{
				packetInfo = userInfo.buffList[i];
				startNo = packetInfo.startNo;
				endNo = packetInfo.endNo;
				pageNum = packetInfo.noPages;
				packetSize = packetInfo.buffSize;

				count++;
				printf("receive %d packet >>> startNo:%d, endNo:%d, pageNum:%d, packetSize:%d\n",count, startNo,endNo,pageNum,packetSize);
//				printf("receive %d packet >>> pageNum:%d, packetSize:%d\n",count, pageNum,packetSize);
				if((count % 10000) == 0)
					printf("have received %d packets\n",count);
#ifdef DEBUG_VERBOSE
				macPacketPrint(readBuffer + startNo * PAGE_SIZE,packetSize);
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
				printf("receive IOCTL FAILED:%d\n ",ret_val);
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
			if( (thread0_exit == 1) && (userInfo.expected ==0))
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
			if( (thread1_exit == 1) && (userInfo.expected ==0))
			{
				if(retry >= 15){
					thread_running[2] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}else if(engine == 2){
			if( (thread2_exit == 1) && (userInfo.expected ==0))
			{
				if(retry >= 15){
					thread_running[4] = 0;
					goto ERROR;
				}
				usleep(100*1000);
				retry++;
			}
		}else if(engine == 3){
			if( (thread3_exit == 1) && (userInfo.expected ==0))
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
	printf("***<thread: receive_data>*** stopped\n");
#ifdef MMap
	if(munmap(readBuffer,PAGE_SIZE * rxInfo.bufferNum) !=0 ){
		printf("munmap failed\n");
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
	int packetSize = test->MaxPktSize;
	int engine = test->Engine;

	buffer = (char *) malloc(packetSize);

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
//	FormatBuffer1(buffer,packetSize,50);
	FormatBuffer2(buffer,packetSize);

	printf("After format, packet is:\n");
	macPacketPrint(buffer, packetSize);


	ret=write(file_desc,buffer,packetSize);
	if(ret < packetSize)
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


