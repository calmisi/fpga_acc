/*
 * setFpgaMode.c
 *
 *  Created on: Mar 13, 2017
 *      Author: root
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>


#include "driverInfo.h"

#define DEBUG_VERBOSE 1

#ifdef DEBUG_VERBOSE /* Enable both normal and verbose logging */
#define log_verbose(args...)    printf(args)
#else
#define log_verbose(x...)
#endif


#define FILENAME "/dev/xdma_stat"

char * ledTransTool(int status){
	return status ?"on":"off";
}

char * intTransTool(int status){
	return (status == 1) ?"yes":"no";
}

int showEngineState(int statsfd, int engine){
	int state[MAX_ENGS][14];
	if(driverInfo_get_EngineState(statsfd, engine, state) != 0){
		printf("get Engine %d state error.\n",engine);
		exit(-1);
	}
	printf("Engine %d engine number is:%d\n",engine,state[engine][0]);
	printf("Engine %d total number of BDs is:%d\n",engine,state[engine][1]);
	printf("Engine %d total number of Buffers is:%d\n",engine,state[engine][2]);
	printf("Engine %d minimum packet size is:%d\n",engine,state[engine][3]);
	printf("Engine %d maximum packet size is:%d\n",engine,state[engine][4]);
	printf("Engine %d total BD errors is:%d\n",engine,state[engine][5]);
	printf("Engine %d total BD short errors is:%d\n",engine,state[engine][6]);
	printf("Engine %d data mismatch error is:%d\n",engine,state[engine][7]);
	printf("Engine %d interrupts enabled state is:%d\n",engine,state[engine][8]);
	printf("Engine %d current test mode is:0x%08x\n",engine,state[engine][9]);
	printf("Engine %d whether in progress is:%s\n",engine,intTransTool(state[engine][10]));
	printf("Engine %d whether in loopback mode is:%s\n",engine,intTransTool(state[engine][11]));
	printf("Engine %d whether in packet generator is:%s\n",engine,intTransTool(state[engine][12]));
	printf("Engine %d whether in packet checker is:%s\n",engine,intTransTool(state[engine][13]));

	return 0;
}

int showLedStatus(int statsfd){
	int fd;
	LedStats ledStats;

	if(fd > 0){
		driverInfo_LedStats(statsfd, &ledStats);
		printf("ddr1 is: %s\n",ledTransTool(ledStats.DdrCalib0));
		printf("ddr2 is: %s\n",ledTransTool(ledStats.DdrCalib1));
		printf("phy0 is: %s\n",ledTransTool(ledStats.Phy0));
		printf("phy1 is: %s\n",ledTransTool(ledStats.Phy1));
		printf("phy2 is: %s\n",ledTransTool(ledStats.Phy2));
		printf("phy3 is: %s\n",ledTransTool(ledStats.Phy3));
	}else{
		printf("error\n");
		return -1;
	}
	return 0;
}

void showDMAStats(int statsfd, int engine){
	float dmaStats[MAX_ENGS][4];
	if(driverInfo_get_DMAStats(statsfd, dmaStats) != 0){
		printf("get dma stats error!\n");
	}

	printf("Engine number is:%d\n",(int)dmaStats[engine][0]);
	printf("Last Byte Rate is:%f\n",dmaStats[engine][1]);
	printf("Last Active Time(ns) is:%f\n",dmaStats[engine][2]);
	printf("Last Wait Time(ns) is:%f\n",dmaStats[engine][3]);


	return;
}

int main(){
	int fd,maxSize;
	int tmode = ENABLE_LOOPBACK;
//	int tmode = ENABLE_PKTCHK;
//	int tmode = ENABLE_PKTGEN;
//	int tmode = ENABLE_PKTCHK|ENABLE_PKTGEN;
	maxSize = 230;


	int control;

	fd = driverInfo_init();
	if(fd < 0){
		printf("cannot open file /dev/xdma_stat.\n");
		exit(-1);
	}

SWITCH:
	printf("1.Set FPGA to Loopback mode\n"
			"2.Unset FPGA\n"
			"3.show engine state\n"
			"4.show DMA state\n"
			"5.show led status\n"
			"0.exit		(enter your option):");
	scanf("%d",&control);
	getchar();
	printf("====================================\n");

	switch(control){
	case 1:
		if(driverInfo_startTest(fd, 0, tmode, maxSize) == -1){
			printf("initialize FPGA error.\n");
			exit(-1);
		}else{
			printf("initialize FPGA to mode %x.\n",tmode);
		}
		break;
	case 2:
		if(driverInfo_stopTest(fd, 0, tmode, maxSize) == 0){
			printf("stop FPGA in mode %x.\n",tmode);
		}else{
			printf("Unset FPGA error.\n");
			exit(-1);
		}
		break;
	case 3:
		showEngineState(fd, 0);
		printf("\n");
		showEngineState(fd, 1);
		break;
	case 4:
		showDMAStats(fd,0);
		break;
	case 5:
		showLedStatus(fd);
		break;
	case 0:
		goto EXIT;
		break;
	default:
		goto SWITCH;
	}
	printf("====================================\n\n");
	goto SWITCH;

EXIT:
	driverInfo_flush(fd);
	printf("\n====================================\n");
	printf("exit");
	return 0;
}
