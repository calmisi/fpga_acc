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
** \   \  /  \   Virtex-7 PCIe-DMA-DDR3-10GMAC-10GBASER Targeted Reference Design
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
 * @file com_xilinx_gui_DriverInfoGen.cpp  
 *
 * Author: Xilinx, Inc.
 *
 * 2007-2010 (c) Xilinx, Inc. This file is licensed uner the terms of the GNU
 * General Public License version 2.1. This program is licensed "as is" without
 * any warranty of any kind, whether express or implied.
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Date     Changes
 * ----- -------- -------------------------------------------------------
 * 1.0  5/15/12  First release
 *
 *****************************************************************************/

#include "driverInfo.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define FILENAME "/dev/xdma_stat"

#define MAX_STATS 350
#define MULTIPLIER  8
#define DIVISOR     (1024*1024*1024)    /* Graph is in Gbits/s */

struct
{
    int Engine;         /* Engine number - for communicating with driver */
    //char *name;         /* Name to be used in Setup screen */
    //int mode;           /* TX/RX - incase of specific screens */
} DMAConfig[MAX_ENGS] =
{
    {0/*, LABEL1, TX_MODE*/ },
    {32/*, LABEL1, RX_MODE*/ },
    {1/*, LABEL2, TX_MODE*/ },
    {33/*, LABEL2, RX_MODE*/ },
    {2},
    {34},
    {3},
    {35}
};



int statsfd=-1; 

int driverInfo_init(){
	// Read the driver file
	if((statsfd = open(FILENAME, O_RDONLY)) < 0)
	{
		printf("Failed to open statistics file %s\n", FILENAME);
		return -1;
	}else{
		return statsfd;
	}

}

int driverInfo_flush(int __fd){
	return close(__fd);
}


int driverInfo_startTest(int __fd, int engine, int testmode, int maxsize){
	int retval=-1;
	TestCmd testCmd;

	testCmd.Engine = engine;
	testCmd.TestMode = testmode;
	testCmd.MaxPktSize = maxsize;
	testCmd.TestMode |= TEST_START;

	printf("mode is %x engine is %d\n", testCmd.TestMode,testCmd.Engine);
	printf("statsfd %d\n", statsfd);

	retval = ioctl(__fd, ISTART_TEST, &testCmd);
	if(retval != 0)
	{
		printf("Error");
		return -1;
	}

	retval = 0;
	return retval;
}

int driverInfo_stopTest(int __fd, int engine, int testmode, int maxsize){
	int retval=-1;
	TestCmd testCmd;

	testCmd.Engine = engine;
	testCmd.TestMode = testmode;
	testCmd.MaxPktSize = maxsize;

	testCmd.TestMode &= ~(TEST_START);
	printf("mode is %x\n", testCmd.TestMode);

	if(ioctl(__fd, ISTOP_TEST, &testCmd) != 0)
	{
		printf("STOP_TEST on engine %d failed\n", engine);
		return -1;
	}

	sleep(3);
	retval = 0;
	return retval;
}


int driverInfo_get_DMAStats(int __fd, float tmp[MAX_ENGS][4]){
	int j,i;

	EngStatsArray es;
	DMAStatistics ds[MAX_STATS];
	es.Count = MAX_STATS;
	es.engptr = ds;

	//float tmp[MAX_ENGS][4];

	if(ioctl(__fd, IGET_DMA_STATISTICS, &es) != 0)
	{
	   // printf("IGET_DMA_STATISTICS on engines failed\n");
		return -1;
	}
	for(j=0; j<es.Count; j++)
	{
		int k, eng;

		/* Driver engine number does not directly map to that of GUI */
		for(k=0; k<MAX_ENGS; k++)
		{
			if(DMAConfig[k].Engine == ds[j].Engine){
				break;
			}
		}

		if(k >= MAX_ENGS) continue;
		eng = k;

		tmp[k][0] = ds[j].Engine;
		tmp[k][1] = ((double)(ds[j].LBR) / DIVISOR ) * MULTIPLIER * ds[j].scaling_factor;
		tmp[k][2] = ds[j].LAT;
		tmp[k][3] = ds[j].LWT;
	 }

	 return 0;
}

int driverInfo_get_TRNStats(int __fd){
	int j, i;
	TRNStatsArray tsa;
	TRNStatistics ts[8];

	tsa.Count = 8;
	tsa.trnptr = ts;

	if(ioctl(__fd, IGET_TRN_STATISTICS, &tsa) != 0)
	{
		//printf("IGET_TRN_STATISTICS failed\n");
		return -1;
	}

	return 0;
}

int driverInfo_get_PowerStats(){
	PowerMonitorVal powerMonitor;
   if(ioctl(statsfd, IGET_PMVAL, &powerMonitor) != 0)
   {
		//printf("IGET_PMVAL failed\n");
		return -1;
   }

   return 0;
}

int driverInfo_get_SWStats(){
	int j, i;
	SWStatsArray ssa;
	SWStatistics ss[32];

	ssa.Count = 32;
	ssa.swptr = ss;

	int tmp[32][2];

	if(ioctl(statsfd, IGET_SW_STATISTICS, &ssa) != 0)
	{
		//printf("IGET_SW_STATISTICS failed\n");
		return -1;
	}
	for(j=0; j<ssa.Count; j++)
	{
		int k, eng;

		/* Driver engine number does not directly map to that of GUI */
		for(k=0; k<MAX_ENGS; k++)
		{
			if(DMAConfig[k].Engine == ss[j].Engine)
				break;
		}

		if(k >= MAX_ENGS) continue;
		eng = k;
		tmp[j][0] = ss[j].Engine;
		tmp[j][1] = ss[j].LBR;
	}
	return 0;
}

int driverInfo_get_EngineState(int __fd, int engineNo, int tmp[MAX_ENGS][14]){
	if (__fd == -1)
	        return -2;

	int i;
	EngState EngInfo;
	int state;

	//int tmp[MAX_ENGS][14];
	/* Get the state of all the engines */
	for(i=0; i<MAX_ENGS; i++)
	{
		EngInfo.Engine = DMAConfig[i].Engine;
		if(ioctl(__fd, IGET_ENG_STATE, &EngInfo) != 0)
		{
			//printf("IGET_ENG_STATE on Engine %d failed\n", EngInfo.Engine);
			for (int k = 0; k < 14; ++k)
			   tmp[i][k] = 0;
		}
		else{
			unsigned int testmode;
			tmp[i][0] = EngInfo.Engine;
			tmp[i][1] = EngInfo.BDs;
			tmp[i][2] = EngInfo.Buffers;
			tmp[i][3] = EngInfo.MinPktSize;
			tmp[i][4] = EngInfo.MaxPktSize;
			tmp[i][5] = EngInfo.BDerrs;
			tmp[i][6] = EngInfo.BDSerrs;
			tmp[i][7] = EngInfo.DataMismatch;
			tmp[i][8] = EngInfo.IntEnab;
			tmp[i][9] = EngInfo.TestMode;
			// These additional ones are for EngStats structure
			testmode = EngInfo.TestMode;
			state = (testmode & (TEST_START|TEST_IN_PROGRESS)) ? 1 : -1;
			tmp[i][10] = state; // EngStats[i].TXEnab
			state = (testmode & ENABLE_LOOPBACK)? 1 : -1;
			tmp[i][11] = state; // EngStats[i].LBEnab
			state = (testmode & ENABLE_PKTGEN)? 1 : -1;
			tmp[i][12] = state; // EngStats[i].PktGenEnab
			state = (testmode & ENABLE_PKTCHK)? 1 : -1;
			tmp[i][13] = state; //EngStats[i].PktChkEnab
		}
	}

	return 0;
}

int driverInfo_get_PCIstate(){
	if (statsfd == -1)
	        return -2;

	 PCIState PCIInfo;
	 // make ioctl call
	 if(ioctl(statsfd, IGET_PCI_STATE, &PCIInfo) != 0)
	 {
		//printf("IGET_PCI_STATE failed\n");
		return -1;
	 }
	 return 0;
}

int driverInfo_setLinkSpeed(int speed){
	DirectLinkChg dLink;

	    dLink.LinkSpeed = 1;   // default
	    dLink.LinkWidth = 1;   // default

	    dLink.LinkSpeed = speed;

	    if(ioctl(statsfd, ISET_PCI_LINKSPEED, &dLink) < 0)
	    {
	        //printf("ISET_PCI_LINKSPEED failed\n");
	        return -1;
	    }
	    return 0;
}

int driverInfo_setLinkWidth(int width){
	DirectLinkChg dLink;

	    dLink.LinkSpeed = 1;   // default
	    dLink.LinkWidth = 1;   // default

	    dLink.LinkWidth = width;

	    if(ioctl(statsfd, ISET_PCI_LINKWIDTH, &dLink) < 0)
	    {
	        //printf("ISET_PCI_LINKWIDTH failed\n");
	        return -1;
	    }
	    return 0;
}

int driverInfo_LedStats(int __fd, LedStats * ledStats){
	 if(ioctl(__fd, IGET_LED_STATISTICS, ledStats) < 0)
	 {
		printf("ISET_PCI_LINKWIDTH failed\n");
		return -1;
	 }

	 return 0;
}

