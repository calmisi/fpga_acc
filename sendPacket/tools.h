/*
 * tools.h
 *
 *  Created on: Mar 8, 2017
 *      Author: root
 */

#ifndef DRIVERINFO_TOOLS_H_
#define DRIVERINFO_TOOLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdio.h>
//////////////////  Thread synchronisation for tx <--> txDone & rx <--> rxDone /////////////////

#define MEM_SUCCESS		0
#define MEM_FAILURE		-1
#define MEM_NOTAVLB     -2

typedef struct MemorySync
{
	unsigned long int iAvailableMemory; /**< Available memory */
	pthread_mutex_t iLock;				/**<  Lock for checking memory */
}MemorySync;

// function uses memorysync structure to init/reserve/free memory availability status
int initMemorySync (MemorySync*, unsigned long int totalMemory);
int ReserveAvailable(MemorySync *, unsigned int requested);
int FreeAvailable(MemorySync *, unsigned int completed);


int xlnx_thread_create(void *fp(void *),void *data);
void hexPrint(const unsigned char *buf, const int num);
void fhexPrint(const FILE * file, const unsigned char * buf, const int num);
void macPacketPrint(const unsigned char * buf, const int num);
void batchedPacketPrint(const unsigned char * buf, const int totalSize, const int perSize);





#ifdef __cplusplus
}
#endif



#endif /* DRIVERINFO_TOOLS_H_ */
