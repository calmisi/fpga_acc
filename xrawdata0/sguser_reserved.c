/*******************************************************************************
 ** © Copyright 2012 - 2013 Xilinx, Inc. All rights reserved.
 ** This file contains confidential and proprietary information of Xilinx, Inc. and 
 ** is protected under U.S. and international copyright and other intellectual property laws.
 *******************************************************************************
 **   ____  ____ 
 **  /   /\/   / 
 ** /___/  \  /   Vendor: Xilinx 
 ** \   \   \/    
 **  \   \
 **  /   /          
 ** /___/    \
 ** \   \  /  \   Virtex-7 FPGA XT Connectivity Targeted Reference Design
 **  \___\/\___\
 ** 
 **  Device: xc7v690t
 **  Version: 1.0
 **  Reference: UG962
 **     
 *******************************************************************************
 **
 **  Disclaimer: 
 **
 **    This disclaimer is not a license and does not grant any rights to the materials 
 **    distributed herewith. Except as otherwise provided in a valid license issued to you 
 **    by Xilinx, and to the maximum extent permitted by applicable law: 
 **    (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS, 
 **    AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, 
 **    INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR 
 **    FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether in contract 
 **    or tort, including negligence, or under any other theory of liability) for any loss or damage 
 **    of any kind or nature related to, arising under or in connection with these materials, 
 **    including for any direct, or any indirect, special, incidental, or consequential loss 
 **    or damage (including loss of data, profits, goodwill, or any type of loss or damage suffered 
 **    as a result of any action brought by a third party) even if such damage or loss was 
 **    reasonably foreseeable or Xilinx had been advised of the possibility of the same.


 **  Critical Applications:
 **
 **    Xilinx products are not designed or intended to be fail-safe, or for use in any application 
 **    requiring fail-safe performance, such as life-support or safety devices or systems, 
 **    Class III medical devices, nuclear facilities, applications related to the deployment of airbags,
 **    or any other applications that could lead to death, personal injury, or severe property or 
 **    environmental damage (individually and collectively, "Critical Applications"). Customer assumes 
 **    the sole risk and liability of any use of Xilinx products in Critical Applications, subject only 
 **    to applicable laws and regulations governing limitations on product liability.

 **  THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT ALL TIMES.

 *******************************************************************************/
/*****************************************************************************/
/**
 *
 * @file sguser.c
 *
 * This is the Application driver which registers with XDMA driver with private interface.
 * This Application driver creates an charracter driver interface with user Application.
 *
 * Author: Xilinx, Inc.
 *
 * 2011-2011 (c) Xilinx, Inc. This file is licensed uner the terms of the GNU
 * General Public License version 2.1. This program is licensed "as is" without
 * any warranty of any kind, whether express or implied.
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Date     Changes
 * ----- -------- -------------------------------------------------------
 * 1.0   05/15/12 First release 
 *
 *****************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/mm.h>		
#include <linux/spinlock.h>	
#include <linux/pagemap.h>	
#include <linux/slab.h>
#include <asm/uaccess.h>


#include <xpmon_be.h>
#include <xdma_user.h>
#include "xdebug.h"
#include "xio.h"

/* Driver states */
#define UNINITIALIZED   0	/* Not yet come up */
#define INITIALIZED     1	/* But not yet set up for polling */
#define UNREGISTERED    2       /* Unregistering with DMA */
#define POLLING         3	/* But not yet registered with DMA */
#define REGISTERED      4	/* Registered with DMA */
#define CLOSED          5	/* Driver is being brought down */

/* DMA characteristics */
#define MYBAR           0

#ifdef XRAWDATA0
#define MYHANDLE  HANDLE_0
#define MYNAME   "Raw Data 0"
#define DEV_NAME  "xraw_data0"
#define DRIVER_NAME         "xrawdata0_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data0 Driver "
#endif

#ifdef XRAWDATA1
#define MYHANDLE  HANDLE_1
#define MYNAME   "Raw Data 1"
#define DEV_NAME  "xraw_data1"
#define DRIVER_NAME         "xrawdata1_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data1 Driver "
#endif

#ifdef XRAWDATA2
#define MYHANDLE  HANDLE_2
#define MYNAME   "Raw Data 2"
#define DEV_NAME  "xraw_data2"
#define DRIVER_NAME         "xrawdata2_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data2 Driver "
#endif

#ifdef XRAWDATA3
#define MYHANDLE  HANDLE_3
#define MYNAME   "Raw Data 3"
#define DEV_NAME  "xraw_data3"
#define DRIVER_NAME         "xrawdata3_driver"
#define DRIVER_DESCRIPTION  "Xilinx Raw Data3 Driver "
#endif


#define DESIGN_MODE_ADDRESS 0x9004		/* Used to configure HW for different modes */        
#ifdef RAW_ETH
#define PERF_DESIGN_MODE   0x00000000
#else
#define PERF_DESIGN_MODE   0x00000003
#endif

#define BURST_SIZE      256

#define XXGE_RCW0_OFFSET        0x00000400 /**< Rx Configuration Word 0 */
#define XXGE_RCW1_OFFSET        0x00000404 /**< Rx Configuration Word 1 */
#define XXGE_TC_OFFSET          0x00000408 /**< Tx Configuration */



#ifdef XRAWDATA0

#define TX_CONFIG_ADDRESS       0x9108
#define RX_CONFIG_ADDRESS       0x9100
#define PKT_SIZE_ADDRESS        0x9104
#define STATUS_ADDRESS          0x910C
#ifndef RAW_ETH
#define SEQNO_WRAP_REG          0x9110
#endif
#endif

#ifdef XRAWDATA1

#define TX_CONFIG_ADDRESS   0x9208	/* Reg for controlling TX data */
#define RX_CONFIG_ADDRESS   0x9200	/* Reg for controlling RX pkt generator */
#define PKT_SIZE_ADDRESS    0x9204	/* Reg for programming packet size */
#define STATUS_ADDRESS      0x920C	/* Reg for checking TX pkt checker status */
#ifndef RAW_ETH
#define SEQNO_WRAP_REG      0x9210  /* Reg for sequence number wrap around */
#endif
#endif

#ifdef XRAWDATA2

#define TX_CONFIG_ADDRESS   0x9308	/* Reg for controlling TX data */
#define RX_CONFIG_ADDRESS   0x9300	/* Reg for controlling RX pkt generator */
#define PKT_SIZE_ADDRESS    0x9304	/* Reg for programming packet size */
#define STATUS_ADDRESS      0x930C	/* Reg for checking TX pkt checker status */
#ifndef RAW_ETH
#define SEQNO_WRAP_REG      0x9310  /* Reg for sequence number wrap around */
#endif
#endif

#ifdef XRAWDATA3

#define TX_CONFIG_ADDRESS   0x9408	/* Reg for controlling TX data */
#define RX_CONFIG_ADDRESS   0x9400	/* Reg for controlling RX pkt generator */
#define PKT_SIZE_ADDRESS    0x9404	/* Reg for programming packet size */
#define STATUS_ADDRESS      0x940C	/* Reg for checking TX pkt checker status */
#ifndef RAW_ETH
#define SEQNO_WRAP_REG      0x9410  /* Reg for sequence number wrap around */
#endif
#endif

/* Test start / stop conditions */
#define PKTCHKR             0x00000001	/* Enable TX packet checker */
#define PKTGENR             0x00000001	/* Enable RX packet generator */
#define CHKR_MISMATCH       0x00000001	/* TX checker reported data mismatch */
#define LOOPBACK            0x00000002	/* Enable TX data loopback onto RX */

#ifdef XRAWDATA0
#define ENGINE_TX       0
#define ENGINE_RX       32
#ifdef RAW_ETH 
#define NW_PATH_OFFSET         0xB000
#define NW_PATH_OFFSET_OTHER   0xC000
#endif
#endif

#ifdef XRAWDATA1
#define ENGINE_TX       1
#define ENGINE_RX       33
#ifdef RAW_ETH 
#define NW_PATH_OFFSET         0xC000
#define NW_PATH_OFFSET_OTHER   0xB000
#endif
#endif


#ifdef XRAWDATA2
#define ENGINE_TX       2
#define ENGINE_RX       34
#ifdef RAW_ETH 
#define NW_PATH_OFFSET         0xD000
#define NW_PATH_OFFSET_OTHER   0xE000
#endif
#endif
 
#ifdef XRAWDATA3
#define ENGINE_TX       3
#define ENGINE_RX       35
#ifdef RAW_ETH 
#define NW_PATH_OFFSET         0xE000
#define NW_PATH_OFFSET_OTHER   0xD000
#endif
#endif

/* Packet characteristics */
#define BUFSIZE         (PAGE_SIZE)
#ifdef RAW_ETH
#define MAXPKTSIZE      (4*PAGE_SIZE - 1)
#else
#define MAXPKTSIZE      (8*PAGE_SIZE)
#endif



#define MINPKTSIZE      (64)
#define NUM_BUFS        100
#define BUFALIGN        8
#define BYTEMULTIPLE    8   /**< Lowest sub-multiple of memory path */


struct cdev *xrawCdev = NULL;
int xraw_DriverState = UNINITIALIZED;
int xraw_UserOpen = 0;

void *handle[4] = { NULL, NULL, NULL, NULL };
#ifdef X86_64
u64 TXbarbase, RXbarbase;
#else
u32 TXbarbase, RXbarbase;
#endif
u32 RawTestMode = TEST_STOP;
u32 RawMinPktSize = MINPKTSIZE, RawMaxPktSize = MAXPKTSIZE;

#ifdef BACK_PRESSURE
#define NO_BP  1
#define YES_BP 2
#define MAX_QUEUE_THRESHOLD 12288
#define MIN_QUEUE_THRESHOLD 8192 
u8 impl_bp = NO_BP;/*back pressure implementation flag */
#endif

typedef struct
{
	int TotalNum;
	int freeNum;
	int headBuf;

	int headReadPtr;
	int tailReadPtr;

	int headAllocPtr;
	int tailAllocPtr;
	unsigned char * origVA[NUM_BUFS];

	struct page * basePage;

	int readable[NUM_BUFS];
} Buffer;

Buffer TxBufs;
Buffer RxBufs;
//char xrawTrans[4096];

/* For exclusion */
spinlock_t RawLock;

/* bufferInfo queue implementation.
 *
 * the variables declared here are only used by either putBufferInfo or getBufferInfo.
 * These should always be guarded by QUpdateLock.
 *
 */
#define MAX_BUFF_INFO 16384

typedef struct BufferInfoQ
{
	spinlock_t iLock;		           /** < will be init to unlock by default  */
	BufferInfo iList[MAX_BUFF_INFO]; /** < Buffer Queue implimented in driver for storing incoming Pkts */
	unsigned int iPutIndex;          /** < Index to put the packets in Queue */
	unsigned int iGetIndex;          /** < Index to get the packets in Queue */ 
	unsigned int iPendingDone;       /** < Indicates number of packets to read */  
} BufferInfoQue;

BufferInfoQue TxDoneQ;		// assuming everything to be initialized to 0 as these are global
BufferInfoQue RxDoneQ;		// assuming everything to be initialized to 0 as these are global


int putBuffInfo (BufferInfoQue * bQue, BufferInfo buff);
int getBuffInfo (BufferInfoQue * bQue, BufferInfo * buff);
#ifdef X86_64
int myInit (u64 barbase, unsigned int );
#else
int myInit (unsigned int barbase, unsigned int );
#endif

static int DmaSetupTransmit (void *, const char __user *, size_t);
static int DmaSetupReceive(void * , int ,const char __user * , size_t );   
int myGetRxPkt (void *, PktBuf *, unsigned int, int, unsigned int);
int myPutTxPkt (void *, PktBuf *, int, unsigned int);
int myPutRxPkt (void *, PktBuf *, int, unsigned int);
int mySetState (void *hndl, UserState * ustate, unsigned int privdata);
int myGetState (void *hndl, UserState * ustate, unsigned int privdata);

/* For checking data integrity */
unsigned int TxBufCnt = 0;
unsigned int RxBufCnt = 0;
unsigned int ErrCnt = 0;

/*	calm define
 * 	allocate a unused buffer from TxBufs or TxBufs
 * */
int initBuf(Buffer * bufs);
unsigned char * AllocBuf(Buffer * bufs);

#define GMULTIPLIER		(1024*1024*1024)
#define MMULTIPLIER		(1024*1024)

//reservedMemory related
static unsigned long Mem_start = 127;	//127G
static unsigned long Mem_size = 8;		//8M = 2k * page_size(4k)
module_param(Mem_start, ulong, S_IRUGO);
module_param(Mem_size, ulong, S_IRUGO);

#ifdef XRAWDATA0
#define MEM_OFFSET (0 * 8 * MMULTIPLIER)
#define RESERVEDMM_IO_DESP "vc709_dma_data0"
#endif

#ifdef XRAWDATA1
#define MEM_OFFSET (1 * 8 * MMULTIPLIER)
#define RESERVEDMM_IO_DESP "vc709_dma_data1"
#endif

#ifdef XRAWDATA2
#define MEM_OFFSET (2 * 8 * MMULTIPLIER)
#define RESERVEDMM_IO_DESP "vc709_dma_data2"
#endif

#ifdef XRAWDATA3
#define MEM_OFFSET (3 * 8 * MMULTIPLIER)
#define RESERVEDMM_IO_DESP "vc709_dma_data3"
#endif

unsigned char * reservedMM_addr;

unsigned long reservedMM_start ;
unsigned long reservedMM_size;


/* end calm define */


int initBuf(Buffer * bufs){
//	unsigned char * bufPages;
	struct page * bufPages;
	int i, offset;

	bufs->TotalNum = 0;
	bufs->freeNum = 0;

	reservedMM_start = Mem_start * GMULTIPLIER + MEM_OFFSET;
	reservedMM_size = Mem_size * MMULTIPLIER;

	//request reserved memory, and ioremap them to get them used
	if(!request_mem_region(reservedMM_start, reservedMM_size, RESERVEDMM_IO_DESP))
				return -EBUSY;
	reservedMM_addr = ioremap(reservedMM_start, reservedMM_size);
	printk(KERN_INFO "reservedMM_addr = 0x%lx\n", (unsigned long)reservedMM_addr);
	if (reservedMM_addr)
	{
		int i;
		for (i = 0; i < Mem_size *1024 * 1024; i += 4)
		{
			reservedMM_addr[i] = 'a';
			reservedMM_addr[i + 1] = 'b';
			reservedMM_addr[i + 2] = 'c';
			reservedMM_addr[i + 3] = 'd';
		}
	}

	//check the ioremap region if it is page-aligned
	offset = offset_in_page(reservedMM_addr);
	if(offset != 0){
		printk(KERN_ERR "Allocated reservedMM is not page aligned, offset in page is:%d",offset);
		return -1;
	}else{
//		bufPages = reservedMM_addr;
//		bufPages = virt_to_page(reservedMM_addr);
		bufPages = pfn_to_page(reservedMM_start >> PAGE_SHIFT);
	}


	//init Rxbufs
	if(bufPages == NULL){
		printk(KERN_ERR "init TX buffs failed, cannot get free page.\n");
		return -1;
	}else{
		bufs->TotalNum = NUM_BUFS;
		bufs->freeNum = NUM_BUFS;
		bufs->headBuf = 0;
		bufs->headAllocPtr = 0;
		bufs->tailAllocPtr =0;
		bufs->headReadPtr =0;
		bufs->tailReadPtr =0;

		bufs->basePage = bufPages;
	}

	for(i = 0; i < NUM_BUFS; i++){
//		bufs->origVA[i] = (unsigned char *)(bufPages + BUFSIZE * i);
		bufs->origVA[i] = (unsigned char *)(bufPages + i);
		printk(KERN_INFO "xrawdata0 get one free page at address bufs->origVA[i] %lx.\n",bufs->origVA[i]);
		bufs->readable[i] =0;
	}

	printk(KERN_INFO "init TX buffs success.\n");
	return 0;
}

unsigned char * AllocBuf(Buffer * bufs){
	int bufPtr;
	bufPtr = bufs->headBuf;

	if(bufs->freeNum > 0){
		bufs->freeNum --;
		bufs->headBuf = (bufs ->headBuf + 1) % NUM_BUFS;
		bufs->headAllocPtr = (bufs->headBuf == 0) ? NUM_BUFS -1 : bufs->headBuf -1;

		printk(KERN_INFO "headAllocPtr is %d, allocate buffer at bufs->origVA[bufPtr] %lx.\n",bufs->headAllocPtr, bufs->origVA[bufPtr]);
		return bufs->origVA[bufPtr];
	}else{
		return NULL;
	}
}

int releaseBuf(Buffer * bufs){
	bufs->freeNum =0;

	/* Not sure if free_page() sleeps or not. */
	spin_lock_bh (&RawLock);
	printk ("Freeing user buffers\n");
	if(reservedMM_addr)
		iounmap(reservedMM_addr);
	release_mem_region(reservedMM_start, reservedMM_size);
	spin_unlock_bh (&RawLock);

	return 0;
}

	static inline void
PrintSummary (void)
{
	u32 val;

	printk ("---------------------------------------------------\n");
	printk ("%s Driver results Summary:-\n", MYNAME);
	printk ("Current Run Min Packet Size = %d, Max Packet Size = %d\n",
			RawMinPktSize, RawMaxPktSize);
	printk
		("Buffers Transmitted = %u, Buffers Received = %u, Error Count = %u\n",
		 TxBufCnt, RxBufCnt, ErrCnt);


	val = XIo_In32 (TXbarbase + STATUS_ADDRESS);
	printk ("Data Mismatch Status = %x\n", val);

	printk ("---------------------------------------------------\n");

}

#ifdef X86_64
int
myInit (u64 barbase, unsigned int privdata)
{
#else
	int
myInit (unsigned int barbase, unsigned int privdata)
{
#endif
	log_normal ("Reached myInit with barbase %x and privdata %x\n",
			barbase, privdata);

	spin_lock_bh (&RawLock);
	if (privdata == 0x54545454)	// So that this is done only once
	{
		TXbarbase = barbase;
	}
	else if (privdata == 0x54545456)	// So that this is done only once
	{
		RXbarbase = barbase;
	}

	TxBufCnt = 0;
	RxBufCnt = 0;
	ErrCnt = 0;


	/* Stop any running tests. The driver could have been unloaded without
	 * stopping running tests the last time. Hence, good to reset everything.
	 */
	XIo_Out32 (TXbarbase + TX_CONFIG_ADDRESS, 0);
	XIo_Out32 (TXbarbase + RX_CONFIG_ADDRESS, 0);

	spin_unlock_bh (&RawLock);

	return 0;
}

	int
myPutRxPkt (void *hndl, PktBuf * vaddr, int numpkts, unsigned int privdata)
{
	int i;
	unsigned int flags;
	PktBuf *pbuf = vaddr;
	static int pktSize;
	static unsigned char *usrAddr = NULL;
	BufferInfo tempBuffInfo;
	static int noPages;

	int bufNo;
	//start calm add
	static int startNo = 0;
	static int endNo = 0;
	//end calm add

	/* Check driver state */
	if (xraw_DriverState != REGISTERED)
	{
		printk ("Driver does not seem to be ready\n");
		return -1;
	}

	/* Check handle value */
	if (hndl != handle[2])
	{
		log_normal ("Came with wrong handle %x\n", (u32) hndl);
		return -1;
	}


	for (i = 0; i < numpkts; i++)
	{
		flags = vaddr->flags;

		pbuf = vaddr;

		if(flags & PKT_UNUSED){
			log_verbose("myRxPutPkt: buffer returned unused\n");
			continue;
		}
		pktSize = pktSize + pbuf->size;

		bufNo = (pbuf->bufInfo - (unsigned char *)RxBufs.basePage)/sizeof(struct page);

		printk("myPutRxPkt put buf, bufNo is %d, packet at page %p, at address %p.\n",bufNo, pbuf->bufInfo, page_address(pbuf->bufInfo));
		RxBufs.tailAllocPtr = (bufNo +1) % NUM_BUFS;

		RxBufs.headReadPtr = bufNo;
		RxBufs.readable[bufNo] = 1;

		if (flags & PKT_SOP)
		{
			usrAddr = pbuf->bufInfo;
			pktSize = pbuf->size;

			startNo = bufNo;
		}
		noPages++;
		if (flags & PKT_EOP)
		{
			//start calm add
			endNo = bufNo;
			tempBuffInfo.startNo = startNo;
			tempBuffInfo.endNo = endNo;
			//stop calm add

			tempBuffInfo.bufferAddress = usrAddr;
			tempBuffInfo.buffSize = pktSize;
			tempBuffInfo.noPages= noPages ;  
			tempBuffInfo.endAddress= pbuf->bufInfo;
			tempBuffInfo.endSize=pbuf->size;
			/* put the packet in driver queue*/
//			printk(KERN_INFO "myPutRxPkt packet at address %p\n",usrAddr);
			putBuffInfo (&RxDoneQ, tempBuffInfo);
			RxBufCnt++;
			pktSize = 0;
			noPages=0;
			usrAddr = NULL;
		}

		unsigned char * bufferAddress = page_address(pbuf->bufInfo);
		int h;
		printk("<0>");
		for(h = 0; h < pbuf->size; h++)
		{
			printk("%02X ", bufferAddress[h]);
			if ((h+1)% 50 == 0)
				printk("\n");
		}
		printk("\n");

		vaddr++;
	}

	/* Return packet buffers to free pool */

	return 0;
}

	int
myGetRxPkt (void *hndl, PktBuf * vaddr, unsigned int size, int numpkts,
		unsigned int privdata)
{
	unsigned char *bufVA;
	PktBuf *pbuf;
	int i;

	log_verbose(KERN_INFO "myGetRxPkt: Came with handle %p size %d privdata %x\n",
			hndl, size, privdata);

	/* Check driver state */
	if (xraw_DriverState != REGISTERED)
	{
		printk ("Driver does not seem to be ready\n");
		return 0;
	}

	/* Check handle value */
	if (hndl != handle[2])
	{
		printk ("Came with wrong handle\n");
		return 0;
	}

	/* Check size value */
	if (size != BUFSIZE)
		printk ("myGetRxPkt: Requested size %d does not match expected %d\n",
				size, (u32) BUFSIZE);

	spin_lock_bh (&RawLock);

	/* Get free buffers from AllocBuf*/
	for (i = 0; i < numpkts; i++)
	{
		pbuf = &(vaddr[i]);
		/* Allocate a buffer. DMA driver will map to PCI space. */
		bufVA = AllocBuf (&RxBufs);

		log_verbose (KERN_INFO
				"myGetRxPkt: The buffer after alloc is at address %x size %d\n",
				(u32) bufVA, (u32) BUFSIZE);
		if (bufVA == NULL)
		{
			log_normal (KERN_ERR "RX: AllocBuf failed\n");
			break;
		}

		printk(KERN_INFO "myGetRxPkt get one buffer at address %lx.\n",bufVA);

		pbuf->pktBuf = bufVA;
		pbuf->bufInfo = bufVA;
		pbuf->size = BUFSIZE;
	}
	spin_unlock_bh (&RawLock);

	log_verbose (KERN_INFO "Requested %d, allocated %d buffers\n", numpkts, i);
	return i;
}

	int
myPutTxPkt (void *hndl, PktBuf * vaddr, int numpkts, unsigned int privdata)
{
	int i;
	unsigned int flags;
	PktBuf *pbuf = vaddr;
	static int pktSize;
	unsigned char *usrAddr = NULL;
	BufferInfo tempBuffInfo;

	log_verbose (KERN_INFO
			"Reached myPutTxPkt with handle %p, numpkts %d, privdata %x\n",
			hndl, numpkts, privdata);

	/* Check driver state */
	if (xraw_DriverState != REGISTERED)
	{
		printk ("Driver does not seem to be ready\n");
		return -1;
	}

	/* Check handle value */
	if (hndl != handle[0])
	{
		printk ("Came with wrong handle\n");
		return -1;
	}

	/* Just check if we are on the way out */
	// spin_lock_bh(&RawLock);
	for (i = 0; i < numpkts; i++)
	{
		flags = vaddr->flags;

		pbuf = vaddr;

		if(pbuf->pageAddr)
			page_cache_release( (struct page *)pbuf->pageAddr);

		pktSize = pktSize + pbuf->size;

		if (flags & PKT_SOP)
		{
			usrAddr = pbuf->bufInfo;
			pktSize = pbuf->size;
		}

		if (flags & PKT_EOP)
		{
			tempBuffInfo.bufferAddress = usrAddr;
			tempBuffInfo.buffSize = pktSize;
//			putBuffInfo (&TxDoneQ, tempBuffInfo);
			TxBufCnt++;
			pktSize = 0;
			usrAddr = NULL;
		}

		vaddr++;

	}


	return 0;
}

	int
mySetState (void *hndl, UserState * ustate, unsigned int privdata)
{
	int val;
#ifndef RAW_ETH
	int seqno;
#endif

	static unsigned int testmode;

	log_verbose (KERN_INFO "Reached mySetState with privdata %x\n", privdata);

	/* Check driver state */
	if (xraw_DriverState != REGISTERED)
	{
		printk ("Driver does not seem to be ready\n");
		return EFAULT;
	}

	/* Check handle value */
	if ((hndl != handle[0]) && (hndl != handle[2]))
	{
		printk ("Came with wrong handle\n");
		return EBADF;
	}

	/* Valid only for TX engine */
	if (privdata == 0x54545454)
	{
		spin_lock_bh (&RawLock);

		/* Set up the value to be written into the register */
		RawTestMode = ustate->TestMode;

		if (RawTestMode & TEST_START)
		{
			testmode = 0;
			if (RawTestMode & ENABLE_LOOPBACK)
				testmode |= LOOPBACK;
			if (RawTestMode & ENABLE_PKTCHK)
				testmode |= PKTCHKR;
			if (RawTestMode & ENABLE_PKTGEN)
				testmode |= PKTGENR;
		}
		else
		{
			/* Deliberately not clearing the loopback bit, incase a
			 * loopback test was going on - allows the loopback path
			 * to drain off packets. Just stopping the source of packets.
			 */
			if (RawTestMode & ENABLE_PKTCHK)
				testmode &= ~PKTCHKR;
			if (RawTestMode & ENABLE_PKTGEN)
				testmode &= ~PKTGENR;

			/* enable this if we need to Disable loop back also */
#ifdef USE_LATER
			if (RawTestMode & ENABLE_LOOPBACK)
				testmode &= ~LOOPBACK;
#endif
		}

		printk ("SetState TX with RawTestMode %x, reg value %x\n",
				RawTestMode, testmode);

		/* Now write the registers */
		if (RawTestMode & TEST_START)
		{
			if (!
					(RawTestMode &
					 (ENABLE_PKTCHK | ENABLE_PKTGEN | ENABLE_LOOPBACK)))
			{
				printk ("%s Driver: TX Test Start called with wrong mode %x\n",
						MYNAME, testmode);
				RawTestMode = 0;
				spin_unlock_bh (&RawLock);
				return EBADRQC;
			}

			printk ("%s Driver: Starting the test - mode %x, reg %x\n",
					MYNAME, RawTestMode, testmode);

			/* Next, set packet sizes. Ensure they don't exceed PKTSIZEs */
			RawMinPktSize = ustate->MinPktSize;
			RawMaxPktSize = ustate->MaxPktSize;

			/* Set RX packet size for memory path */
			val = RawMaxPktSize;
			printk ("Reg %x = %x\n", PKT_SIZE_ADDRESS, val);
			RawMinPktSize = RawMaxPktSize = val;
			/* Now ensure the sizes remain within bounds */
			if (RawMaxPktSize > MAXPKTSIZE)
				RawMinPktSize = RawMaxPktSize = MAXPKTSIZE;
			if (RawMinPktSize < MINPKTSIZE)
				RawMinPktSize = RawMaxPktSize = MINPKTSIZE;
			if (RawMinPktSize > RawMaxPktSize)
				RawMinPktSize = RawMaxPktSize;
			val = RawMaxPktSize;
#ifndef RAW_ETH
			log_verbose("========Reg %x = %d\n",DESIGN_MODE_ADDRESS, PERF_DESIGN_MODE);
			XIo_Out32 (TXbarbase + DESIGN_MODE_ADDRESS,PERF_DESIGN_MODE);
			log_verbose("DESIGN MODE %d\n",PERF_DESIGN_MODE );
#endif
			log_verbose("========Reg %x = %d\n", PKT_SIZE_ADDRESS, val);
			XIo_Out32 (TXbarbase + PKT_SIZE_ADDRESS, val);
			log_verbose("RxPktSize %d\n", val);
#ifndef RAW_ETH	  
			seqno= TX_CONFIG_SEQNO;
			log_verbose("========Reg %x = %d\n",SEQNO_WRAP_REG, seqno);
			XIo_Out32 (TXbarbase + SEQNO_WRAP_REG , seqno);
			log_verbose("SeqNo Wrap around %d\n", seqno);
#endif

			mdelay(1); 


			/* Incase the last test was a loopback test, that bit may not be cleared. */
			XIo_Out32 (TXbarbase + TX_CONFIG_ADDRESS, 0);
			if (RawTestMode & (ENABLE_PKTCHK | ENABLE_LOOPBACK))
			{

				log_verbose("========Reg %x = %x\n", TX_CONFIG_ADDRESS, testmode);
				XIo_Out32 (TXbarbase + TX_CONFIG_ADDRESS, testmode);
#ifdef RAW_ETH
				log_verbose("Reg[DESIGN_MODE] = %x\n", XIo_In32(TXbarbase+DESIGN_MODE_ADDRESS));
				XIo_Out32(TXbarbase+DESIGN_MODE_ADDRESS,PERF_DESIGN_MODE);
				log_verbose("Disable performance mode....\nReg[DESIGN_MODE] = %x\n", XIo_In32(TXbarbase+DESIGN_MODE_ADDRESS));

//				if(RawTestMode & ENABLE_CRISCROSS)
//				{
//					printk("XGEMAC-RCW1 = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET_OTHER + XXGE_RCW1_OFFSET));
//					XIo_Out32(TXbarbase+NW_PATH_OFFSET_OTHER+XXGE_RCW1_OFFSET, 0x50000000);
//					printk("XGEMAC-RCW1 = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET_OTHER + XXGE_RCW1_OFFSET));
//				}
//				else
//				{
//					log_verbose("XGEMAC-RCW1 = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET + XXGE_RCW1_OFFSET));
//					XIo_Out32(TXbarbase+NW_PATH_OFFSET+XXGE_RCW1_OFFSET, 0x50000000);
//					log_verbose("XGEMAC-RCW1 = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET + XXGE_RCW1_OFFSET));
//				}
//				log_verbose("XGEMAC-TC = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET + XXGE_TC_OFFSET));
//				XIo_Out32(TXbarbase+NW_PATH_OFFSET+XXGE_TC_OFFSET, 0x50000000);
//				log_verbose("XGEMAC-TC = %x\n", XIo_In32(TXbarbase + NW_PATH_OFFSET + XXGE_TC_OFFSET));
#endif
			}
			if (RawTestMode & ENABLE_PKTGEN)
			{
				log_verbose("========Reg %x = %x\n", RX_CONFIG_ADDRESS, testmode);
				XIo_Out32 (TXbarbase + RX_CONFIG_ADDRESS, testmode);
			}

		}
		/* Else, stop the test. Do not remove any loopback here because
		 * the DMA queues and hardware FIFOs must drain first.
		 */
		else
		{
			printk ("%s Driver: Stopping the test, mode %x\n", MYNAME,
					testmode);
			printk ("========Reg %x = %x\n", TX_CONFIG_ADDRESS, testmode);
			mdelay(50);   
			XIo_Out32 (TXbarbase + TX_CONFIG_ADDRESS, testmode);
			mdelay(10);
			printk ("========Reg %x = %x\n", RX_CONFIG_ADDRESS, testmode);
			XIo_Out32 (TXbarbase + RX_CONFIG_ADDRESS, testmode);
			mdelay(100);


		}

		PrintSummary ();
		spin_unlock_bh (&RawLock);
	}
	return 0;
}

	int
myGetState (void *hndl, UserState * ustate, unsigned int privdata)
{
	static int iter = 0;

	/* Same state is being returned for both engines */

	ustate->LinkState = LINK_UP;
	ustate->DataMismatch= XIo_In32 (TXbarbase + STATUS_ADDRESS);
	ustate->MinPktSize = RawMinPktSize;
	ustate->MaxPktSize = RawMaxPktSize;
	ustate->TestMode = RawTestMode;
	if (privdata == 0x54545454)
		ustate->Buffers = TxBufs.TotalNum;
	else
		ustate->Buffers = RxBufs.TotalNum;

	if (iter++ >= 4)
	{
		PrintSummary ();

		iter = 0;
	}

	return 0;
}


#define QSUCCESS 0
#define QFAILURE -1

/* 
   putBuffInfo is used for adding an buffer element to the queue.
   it updates the queue parameters (by holding QUpdateLock).

Returns: 0 for success / -1 for failure 
*/
int putBuffInfo (BufferInfoQue * bQue, BufferInfo buff)
{

	// assert (bQue != NULL)

	int currentIndex = 0;
	spin_lock_bh (&(bQue->iLock));

	currentIndex = (bQue->iPutIndex + 1) % MAX_BUFF_INFO;

	if (currentIndex == bQue->iGetIndex)
	{
		spin_unlock_bh (&(bQue->iLock));
		printk (KERN_ERR "%s: BufferInfo Q is FULL in %s , drop the incoming buffers",
				__func__,__FILE__);
		return QFAILURE;		// array full
	}

	bQue->iPutIndex = currentIndex;

	bQue->iList[bQue->iPutIndex] = buff;
	bQue->iPendingDone++;
#ifdef BACK_PRESSURE
	if(bQue == &RxDoneQ)
	{
		if((impl_bp == NO_BP)&& ( bQue->iPendingDone > MAX_QUEUE_THRESHOLD))
		{
			impl_bp = YES_BP;
			printk(KERN_ERR "XXXXXX Maximum Queue Threshold reached.Turning on BACK PRESSURE XRAW0 %d  \n",bQue->iPendingDone);
		} 
	}
#endif  
	spin_unlock_bh (&(bQue->iLock));
	return QSUCCESS;
}

/* 
   getBuffInfo is used for fetching the oldest buffer info from the queue.
   it updates the queue parameters (by holding QUpdateLock).

Returns: 0 for success / -1 for failure 
*/
int getBuffInfo (BufferInfoQue * bQue, BufferInfo * buff)
{
	// assert if bQue is NULL
	if (!buff || !bQue)
	{
		printk (KERN_ERR "%s: BAD BufferInfo pointer", __func__);
		return QFAILURE;
	}

	spin_lock_bh (&(bQue->iLock));

	// assuming we get the right buffer
	if (!bQue->iPendingDone)
	{
		spin_unlock_bh (&(bQue->iLock));
		log_verbose(KERN_ERR "%s: BufferInfo Q is Empty",__func__);
		return QFAILURE;
	}

	bQue->iGetIndex++;
	bQue->iGetIndex %= MAX_BUFF_INFO;
	*buff = bQue->iList[bQue->iGetIndex];
	bQue->iPendingDone--;
#ifdef BACK_PRESSURE
	if(bQue == &RxDoneQ) 
	{
		if((impl_bp == YES_BP) && (bQue->iPendingDone < MIN_QUEUE_THRESHOLD))
		{
			impl_bp = NO_BP;
			printk(KERN_ERR "XXXXXXX Minimum Queue Threshold reached.Turning off Back Pressure at %d %s\n",__LINE__,__FILE__);
		}
	}
#endif
	spin_unlock_bh (&(bQue->iLock));

	return QSUCCESS;

}


#define WRITE_TO_CARD   0	
#define READ_FROM_CARD  1	


static int DmaSetupReceive(void * hndl, int num ,const char __user * buffer, size_t length)   
{
	int j;
	int total, result;
	PktBuf * pbuf;
	int status;               
	int offset;                
	unsigned int allocPages;   
	unsigned long first, last; 
	struct page** cachePages; 
	PktBuf **pkts;

	/* Check driver state */
	if(xraw_DriverState != REGISTERED)
	{
		printk("Driver does not seem to be ready\n");
		return 0;
	}

	/* Check handle value */
	if(hndl != handle[2])
	{
		printk("Came with wrong handle\n");
		return 0;
	}

	/* Check number of packets */
	if(!num)
	{
		printk("Came with 0 packets for sending\n");
		return 0;
	}

	total = 0;

	/****************************************************************/
	// SECTION 1: generate CACHE PAGES for USER BUFFER
	//
	offset = offset_in_page(buffer);
	first = ((unsigned long)buffer & PAGE_MASK) >> PAGE_SHIFT;
	last  = (((unsigned long)buffer + length-1) & PAGE_MASK) >> PAGE_SHIFT;
	allocPages = (last-first)+1;


	pkts = kmalloc( allocPages * (sizeof(PktBuf*)), GFP_KERNEL);
	if(pkts == NULL)
	{
		printk(KERN_ERR "Error: unable to allocate memory for pkts\n");
		return -1;
	}

	cachePages = kmalloc( (allocPages * (sizeof(struct page*))), GFP_KERNEL );
	if( cachePages == NULL )
	{
		printk(KERN_ERR "Error: unable to allocate memory for cachePages\n");
		kfree(pkts);
		return -1;
	}

	memset(cachePages, 0, sizeof(allocPages * sizeof(struct page*)) );
	down_read(&(current->mm->mmap_sem));
	status = get_user_pages(current,        // current process id
			current->mm,                // mm of current process
			(unsigned long)buffer,      // user buffer
			allocPages,
			READ_FROM_CARD,
			0,                          /* don't force */
			cachePages,
			NULL);
	up_read(&current->mm->mmap_sem);
	if( status < allocPages) {
		printk(KERN_ERR ".... Error: requested pages=%d, granted pages=%d ....\n", allocPages, status);

		for(j=0; j<status; j++)
			page_cache_release(cachePages[j]);

		kfree(pkts);
		kfree(cachePages);
		return -1;
	}


	allocPages = status;	// actual number of pages system gave

	for(j=0; j< allocPages; j++)		/* Packet fragments loop */
	{
		pbuf = kmalloc( (sizeof(PktBuf)), GFP_KERNEL);

		if(pbuf == NULL) {
			printk(KERN_ERR "Insufficient Memory !!\n");
			for(j--; j>=0; j--)
				kfree(pkts[j]);
			for(j=0; j<allocPages; j++)
				page_cache_release(cachePages[j]);
			kfree(pkts);
			kfree(cachePages);
			return -1;
		}

		//spin_lock_bh(&RawLock);
		pkts[j] = pbuf;

		// first buffer would start at some offset, need not be on page boundary
		if(j==0) {
			pbuf->size = ((PAGE_SIZE)-offset);
		} 
		else {
			if(j == (allocPages-1)) { 
				pbuf->size = length-total;
			}
			else pbuf->size = (PAGE_SIZE);
		}
		pbuf->pktBuf = (unsigned char*)cachePages[j];		

		pbuf->bufInfo = (unsigned char *) buffer + total;

		pbuf->pageAddr= (unsigned char*)cachePages[j];

		pbuf->flags = PKT_ALL;

		total += pbuf->size;
		//spin_unlock_bh(&RawLock);
	}
	/****************************************************************/

	allocPages = j;           // actually used pages

	result = DmaSendPages(hndl, pkts, allocPages);
	if(result == -1)
	{
		for(j=0; j<allocPages; j++) {
			page_cache_release(cachePages[j]);
		}

		total = 0;
	}
	kfree(cachePages);

	for(j=0; j<allocPages; j++) {
		kfree(pkts[j]);
	}
	kfree(pkts);

	return total;
}
static int DmaSetupTransmit(void * hndl,const char __user * buffer, size_t length)
{
	int j;
	int total, result;
	PktBuf * pbuf;
	int status;                
	int offset;                
	unsigned int allocPages;   
	unsigned long first, last; 
	struct page** cachePages;  
	PktBuf **pkts;

	/* Check driver state */
	if(xraw_DriverState != REGISTERED)
	{
		printk("Driver does not seem to be ready\n");
		return 0;
	}

	/* Check handle value */
	if(hndl != handle[0])
	{
		printk("Came with wrong handle\n");
		return 0;
	}

	total = 0;
	/****************************************************************/
	// SECTION 1: generate CACHE PAGES for USER BUFFER
	offset = offset_in_page(buffer);
	first = ((unsigned long)buffer & PAGE_MASK) >> PAGE_SHIFT;
	last  = (((unsigned long)buffer + length-1) & PAGE_MASK) >> PAGE_SHIFT;
	allocPages = (last-first)+1;

	pkts = kmalloc( allocPages * (sizeof(PktBuf*)), GFP_KERNEL);
	if(pkts == NULL)
	{
		printk(KERN_ERR "Error: unable to allocate memory for packets\n");
		return -1;
	}

	cachePages = kmalloc( (allocPages * (sizeof(struct page*))), GFP_KERNEL );
	if( cachePages == NULL )
	{
		printk(KERN_ERR "Error: unable to allocate memory for cachePages\n");
		kfree(pkts);
		return -1;
	}

	memset(cachePages, 0, sizeof(allocPages * sizeof(struct page*)) );
	down_read(&(current->mm->mmap_sem));
	status = get_user_pages(current,        // current process id
			current->mm,                // mm of current process
			(unsigned long)buffer,      // user buffer
			allocPages,
			WRITE_TO_CARD,
			0,                          /* don't force */
			cachePages,
			NULL);
	up_read(&current->mm->mmap_sem);
	if( status < allocPages) {
		printk(KERN_ERR ".... Error: requested pages=%d, granted pages=%d ....\n", allocPages, status);

		for(j=0; j<status; j++)
			page_cache_release(cachePages[j]);
		kfree(pkts);
		kfree(cachePages);
		return -1;
	}


	allocPages = status;	// actual number of pages system gave

	for(j=0; j< allocPages; j++)		/* Packet fragments loop */
	{
		pbuf = kmalloc( (sizeof(PktBuf)), GFP_KERNEL);

		if(pbuf == NULL) {
			printk(KERN_ERR "Insufficient Memory !!\n");
			for(j--; j>=0; j--)
				kfree(pkts[j]);
			for(j=0; j<allocPages; j++)
				page_cache_release(cachePages[j]);
			kfree(pkts);
			kfree(cachePages);
			return -1;
		}

		//spin_lock_bh(&RawLock);
		pkts[j] = pbuf;

		// first buffer would start at some offset, need not be on page boundary
		if(j==0) {
			if(j == (allocPages-1)) { 
				pbuf->size = length;
			}
			else
				pbuf->size = ((PAGE_SIZE)-offset);
		} 
		else {
			if(j == (allocPages-1)) { 
				pbuf->size = length-total;
			}
			else pbuf->size = (PAGE_SIZE);
		}
		pbuf->pktBuf = (unsigned char*)cachePages[j];		

		pbuf->pageOffset = (j == 0) ? offset : 0;	// try pci_page_map

		pbuf->bufInfo = (unsigned char *) buffer + total;
		pbuf->pageAddr= (unsigned char*)cachePages[j];
		pbuf->userInfo = length;

		pbuf->flags = PKT_ALL;
		if(j == 0)
		{
			pbuf->flags |= PKT_SOP;
		}
		if(j == (allocPages - 1) )
		{
			pbuf->flags |= PKT_EOP;
		}
		total += pbuf->size;
		//spin_unlock_bh(&RawLock);
	}
	/****************************************************************/

	allocPages = j;           // actually used pages

	result = DmaSendPages_Tx (hndl, pkts,allocPages);
	if(result == -1)
	{
		for(j=0; j<allocPages; j++) {
			page_cache_release(cachePages[j]);
		}
		total = 0;
	}
	kfree(cachePages);

	for(j=0; j<allocPages; j++) {
		kfree(pkts[j]);
	}
	kfree(pkts);

	return total;
}

//static int CPU_LOADED[16] =
//{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static int CPU_LOADED[20] =
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	static int
xraw_dev_open (struct inode *in, struct file *filp)
{
	int cpu_id = 0;

	if (xraw_DriverState < INITIALIZED)
	{
		printk ("Driver not yet ready!\n");
		return -1;
	}

	cpu_id = get_cpu ();
	if (CPU_LOADED[cpu_id] == 0)
	{
		CPU_LOADED[cpu_id] = 1;
	}
	else
	{
		printk (KERN_ERR "CPU %d is already loaded, exit this process\n",
				cpu_id);
		//return -1;
	}
	printk (KERN_ERR "$$$$$$ CPU ID %d $$$$$$\n", cpu_id);


	//  if (xraw_UserOpen)
	//    {                                          /* To prevent more than one GUI */
	//      printk ("Device already in use\n");
	//      return -EBUSY;
	//    }

	//spin_lock_bh(&DmaStatsLock);
	xraw_UserOpen++;		  
	//spin_unlock_bh(&DmaStatsLock);
	printk ("========>>>>> XDMA driver instance %d \n", xraw_UserOpen);

	return 0;
}

	static int
xraw_dev_release (struct inode *in, struct file *filp)
{
	int cpu_id = 0;

	if (!xraw_UserOpen)
	{
		/* Should not come here */
		printk ("Device not in use\n");
		return -EFAULT;
	}

	//  spin_lock_bh(&DmaStatsLock);
	xraw_UserOpen--;
	//  spin_unlock_bh(&DmaStatsLock);

	cpu_id = get_cpu ();
	CPU_LOADED[cpu_id] = 0;
	printk (KERN_ERR "CPU %d is released\n", cpu_id);

	return 0;
}

	static long
xraw_dev_ioctl (struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	if (xraw_DriverState < INITIALIZED)
	{
		/* Should not come here */
		printk ("Driver not yet ready!\n");
		return -1;
	}

	/* Check cmd type and value */
	if (_IOC_TYPE (cmd) != XPMON_MAGIC)
		return -ENOTTY;
	if (_IOC_NR (cmd) > XPMON_MAX_CMD)
		return -ENOTTY;

	/* Check read/write and corresponding argument */
	if (_IOC_DIR (cmd) & _IOC_READ)
		if (!access_ok (VERIFY_WRITE, (void *) arg, _IOC_SIZE (cmd)))
			return -EFAULT;
	if (_IOC_DIR (cmd) & _IOC_WRITE)
		if (!access_ok (VERIFY_READ, (void *) arg, _IOC_SIZE (cmd)))
			return -EFAULT;

	switch (cmd)
	{
		case IGET_RX_PACKETINFO:
			{
				int bufNumber = RxBufs.TotalNum;
				int freeNum = -1;

//				printk(KERN_INFO "RxBufs.freeNum: %d\n",RxBufs.freeNum);
				if(copy_from_user(&freeNum, &(((RxPacketInfo *)arg)->freeNum),sizeof(int)) !=0  )
				{
					printk ("##### ERROR in copy from usr #####");
					break;
				}
				//if user application have read the data,then free the RxBuf to be used by RX engine
				if(freeNum > 0){
					RxBufs.freeNum += freeNum;
					RxBufs.tailAllocPtr = (RxBufs.tailAllocPtr + freeNum) % NUM_BUFS;
//					printk(KERN_INFO"get freeNum %d,RxBufs.freeNum: %d\n",freeNum,RxBufs.freeNum);
				}else if(freeNum == 0){
					if(copy_to_user(&(((RxPacketInfo *)arg)->bufferNum),&bufNumber,sizeof(int)) != 0 )
					{
						printk ("##### ERROR in copy to usr #####");
					}
				}

				break;
			}

		case IGET_TRN_TXUSRINFO:
			{
				int count = 0;

				int expect_count;
				if(copy_from_user(&expect_count,&(((FreeInfo *)arg)->expected),sizeof(int)) != 0)
				{
					printk ("##### ERROR in copy from usr #####");
					break;
				}
				while (count < expect_count)
				{
					BufferInfo buff;
					if (0 != getBuffInfo (&TxDoneQ, &buff))
					{
						break;
					}
					if (copy_to_user
							(((BufferInfo *) (((FreeInfo *)arg)->buffList) + count), &buff,
							 sizeof (BufferInfo)))
					{
						printk ("##### ERROR in copy to usr #####");

					}
					// log_verbose(" %s:bufferAddr %x   PktSize %d", __func__, usrArgument->buffList[count].bufferAddress, usrArgument->buffList[count].buffSize);
					count++;
				}
				if(copy_to_user(&(((FreeInfo *)arg)->expected),&count,(sizeof(int))) != 0)
				{
					printk ("##### ERROR in copy to usr #####");
				}

				break;
			}
		case IGET_TRN_RXUSRINFO:
			{
				int count = 0;
				int expect_count;

				if(copy_from_user(&expect_count,&(((FreeInfo *)arg)->expected),sizeof(int)) != 0)
				{
					printk ("##### ERROR in copy from usr #####");
					break;
				}

				while (count < expect_count)
				{
					BufferInfo buff;
					if (0 != getBuffInfo (&RxDoneQ, &buff))
					{
						break;
					}
					if (copy_to_user
							(((BufferInfo *) (((FreeInfo *)arg)->buffList) + count), &buff,
							 sizeof (BufferInfo)))
					{
						printk ("##### ERROR in copy to usr #####");

					}
					log_verbose(" %s:bufferAddr %x   PktSize %d", __func__, usrArgument->buffList[count].bufferAddress, usrArgument->buffList[count].buffSize);
					count++;
				}
				if(copy_to_user(&(((FreeInfo *)arg)->expected),&count,(sizeof(int))) != 0)
				{
					printk ("##### ERROR in copy to usr #####");
				}

				break;
			}
		default:
			printk ("Invalid command %d\n", cmd);
			retval = -1;
			break;
	}

	return retval;
}

/* 
 * This function is called when somebody tries to
 * write into our device file. 
 */
	static ssize_t
xraw_dev_write (struct file *file,
		const char __user * buffer, size_t length, loff_t * offset)
{
	int ret_pack=0;


	if ((RawTestMode & TEST_START) &&
			(RawTestMode & (ENABLE_PKTCHK | ENABLE_LOOPBACK)))
		ret_pack = DmaSetupTransmit(handle[0], buffer, length);

	/* 
	 *  return the number of bytes sent , currently one or none
	 */
	return ret_pack;
}

	static ssize_t
xraw_dev_read (struct file *file,
		char __user * buffer, size_t length, loff_t * offset)
{
	int ret_pack=0;

#ifdef BACK_PRESSURE
	if(impl_bp == NO_BP)
#endif
		ret_pack = DmaSetupReceive(handle[2],1,buffer,length);


	/* 
	 *  return the number of bytes sent , currently one or none
	 */

	return ret_pack;
}

	static int
xraw_dev_mmap(struct file * file, struct vm_area_struct * vma){
		struct page * RxbufferStart;
		RxbufferStart =(struct page *) RxBufs.basePage;
		unsigned long reservedMM_start = Mem_start * GMULTIPLIER;

		unsigned long size = vma->vm_end - vma->vm_start;

		if(size > Mem_size*MMULTIPLIER){
			printk(DEV_NAME ":mmap() quested size is too big\n");
			return ( - ENXIO);
		}

		if(remap_pfn_range(vma, vma->vm_start,
//				page_to_pfn(RxbufferStart),
				reservedMM_start>>PAGE_SHIFT,
				vma->vm_end -vma->vm_start,
				vma->vm_page_prot))
			return -EAGAIN;

//		vma->vm_flags |= VM_RESERVED;
		return 0;

		printk(KERN_INFO "page_to_pfn(RxbufferStart) is %lx, reservedMM_start>>PAGE_SHIFT is %lx",page_to_pfn(RxbufferStart),reservedMM_start>>PAGE_SHIFT);
	}


	static int __init
rawdata_init (void)
{
	int chrRet;
	dev_t xrawDev;
	UserPtrs ufuncs;
	static struct file_operations xrawDevFileOps;

	/* Just register the driver. No kernel boot options used. */
	printk (KERN_INFO "%s Init: Inserting Xilinx driver in kernel.\n", MYNAME);

  xraw_DriverState = INITIALIZED;
  spin_lock_init (&RawLock);
  spin_lock_init (&(TxDoneQ.iLock));
  spin_lock_init (&(RxDoneQ.iLock));


	msleep (5);

	/* First allocate a major/minor number. */
	chrRet = alloc_chrdev_region (&xrawDev, 0, 1, DEV_NAME);
	if (IS_ERR ((int *) chrRet))
	{
		log_normal (KERN_ERR "Error allocating char device region\n");
		return -1;
	}
	else
	{
		/* Register our character device */
		xrawCdev = cdev_alloc ();
		if (IS_ERR (xrawCdev))
		{
			log_normal (KERN_ERR "Alloc error registering device driver\n");
			unregister_chrdev_region (xrawDev, 1);
			return -1;
		}
		else
		{
			xrawDevFileOps.owner = THIS_MODULE;
			xrawDevFileOps.open = xraw_dev_open;
			xrawDevFileOps.release = xraw_dev_release;
			xrawDevFileOps.unlocked_ioctl = xraw_dev_ioctl;
			xrawDevFileOps.write = xraw_dev_write;
			xrawDevFileOps.read = xraw_dev_read;
			xrawDevFileOps.mmap = xraw_dev_mmap;
			xrawCdev->owner = THIS_MODULE;
			xrawCdev->ops = &xrawDevFileOps;
			xrawCdev->dev = xrawDev;
			chrRet = cdev_add (xrawCdev, xrawDev, 1);
			if (chrRet < 0)
			{
				log_normal (KERN_ERR "Add error registering device driver\n");
				cdev_del(xrawCdev);
				unregister_chrdev_region (xrawDev, 1);
				return -1;
			}
		}
	}

	if (!IS_ERR ((int *) chrRet))
	{
		printk (KERN_INFO "Device registered with major number %d\n",
				MAJOR (xrawDev));

	}

	xraw_DriverState = INITIALIZED;
	/* Register with DMA incase not already done so */
	if (xraw_DriverState < POLLING)
	{
		spin_lock_bh (&RawLock);
		printk ("Calling DmaRegister on engine %d and %d\n",
				ENGINE_TX, ENGINE_RX);
		xraw_DriverState = REGISTERED;

		ufuncs.UserInit = myInit;
		ufuncs.UserPutPkt = myPutTxPkt;
		ufuncs.UserSetState = mySetState;
		ufuncs.UserGetState = myGetState;
#ifdef PM_SUPPORT
		ufuncs.UserSuspend_Early = NULL;
		ufuncs.UserSuspend_Late = NULL;
		ufuncs.UserResume = NULL;
#endif
		ufuncs.privData = 0x54545454;
#ifdef RAW_ETH
		ufuncs.mode = RAWETHERNET_MODE;
#else
		ufuncs.mode = PERFORMANCE_MODE;
#endif
		spin_unlock_bh (&RawLock);

		if ((handle[0] =
					DmaRegister (ENGINE_TX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
		{
			printk ("Register for engine %d failed. Stopping.\n", ENGINE_TX);
			spin_lock_bh (&RawLock);
			xraw_DriverState = UNREGISTERED;
			spin_unlock_bh (&RawLock);
			cdev_del(xrawCdev);
			unregister_chrdev_region (xrawDev, 1);
			return -1;		
		}
		printk ("Handle for engine %d is %p\n", ENGINE_TX, handle[0]);

		spin_lock_bh (&RawLock);
		ufuncs.UserInit = myInit;
		ufuncs.UserPutPkt = myPutRxPkt;
		ufuncs.UserGetPkt = myGetRxPkt;
		ufuncs.UserSetState = mySetState;
		ufuncs.UserGetState = myGetState;
#ifdef PM_SUPPORT
		ufuncs.UserSuspend_Early = NULL;
		ufuncs.UserSuspend_Late = NULL;
		ufuncs.UserResume = NULL;
#endif
		ufuncs.privData = 0x54545456;
#ifdef RAW_ETH
		ufuncs.mode = RAWETHERNET_MODE;
#else
		ufuncs.mode = PERFORMANCE_MODE;
#endif
		spin_unlock_bh (&RawLock);

		//alloc buffer for RxBufs
		if(initBuf(&RxBufs) < 0){
				return -1;
			}

		if ((handle[2] =
					DmaRegister (ENGINE_RX, MYBAR, &ufuncs, BUFSIZE)) == NULL)
		{
			printk ("Register for engine %d failed. Stopping.\n", ENGINE_RX);
			spin_lock_bh (&RawLock);
			xraw_DriverState = UNREGISTERED;
			spin_unlock_bh (&RawLock);
			cdev_del(xrawCdev);
			unregister_chrdev_region (xrawDev, 1);
			return -1;		
		}
		printk ("Handle for engine %d is %p\n", ENGINE_RX, handle[2]);


	}

	return 0;
}

	static void __exit
rawdata_cleanup (void)
{
	int i;

	/* Stop any running tests, else the hardware's packet checker &
	 * generator will continue to run.
	 */
	XIo_Out32 (TXbarbase + TX_CONFIG_ADDRESS, 0);

	XIo_Out32 (TXbarbase + RX_CONFIG_ADDRESS, 0);
#ifdef RAW_ETH
//	mdelay(1);
//
//  XIo_Out32(TXbarbase+(u32)(NW_PATH_OFFSET+XXGE_TC_OFFSET), 0x80000000);
//  mdelay(1);
//
//  XIo_Out32(TXbarbase+(u32)(NW_PATH_OFFSET+XXGE_RCW1_OFFSET), 0x80000000);
//  mdelay(1);
#endif

	printk (KERN_INFO "%s: Unregistering Xilinx driver from kernel.\n", MYNAME);
//	if (TxBufCnt != RxBufCnt)
//	{
//		printk ("%s: Buffers Transmitted %u Received %u\n", MYNAME, TxBufCnt,
//				RxBufCnt);
//
//		mdelay (1);
//	}
#ifdef FIFO_EMPTY_CHECK
	DmaFifoEmptyWait(MYHANDLE,DIR_TYPE_S2C);
	// wait for appropriate time to stabalize
	mdelay(STABILITY_WAIT_TIME);
#endif
	DmaUnregister (handle[0]);
#ifdef FIFO_EMPTY_CHECK
	DmaFifoEmptyWait(MYHANDLE,DIR_TYPE_C2S);
	// wait for appropriate time to stabalize
	mdelay(STABILITY_WAIT_TIME);
#endif
	DmaUnregister (handle[2]);


	PrintSummary ();
	/*Unregistering the char driver  */
	if (xrawCdev != NULL)
	{
		printk ("Unregistering char device driver\n");
		cdev_del (xrawCdev);
		unregister_chrdev_region (xrawCdev->dev, 1);
	}

	mdelay (1000);

	releaseBuf(&RxBufs);
//	/* Not sure if free_page() sleeps or not. */
//	spin_lock_bh (&RawLock);
//	printk ("Freeing user buffers\n");
//	RxBufs.freeNum = 0;
//	for (i = 0; i < RxBufs.TotalNum; i++)
//		__free_page (RxBufs.origVA[i]);
//	spin_unlock_bh (&RawLock);
}

module_init (rawdata_init);
module_exit (rawdata_cleanup);

MODULE_AUTHOR ("Xilinx, Inc.");
MODULE_DESCRIPTION (DRIVER_DESCRIPTION);
MODULE_LICENSE ("GPL");
