/*
 * tools.cpp
 *
 *  Created on: Mar 8, 2017
 *      Author: root
 */

#include <errno.h>
#include <string.h>

#include "tools.h"

int prChunkSize = 32;

int xlnx_thread_create(void *fp(void *),void *data)
{
	pthread_attr_t        attr;
	pthread_t             thread;
	int                   rc=0;

	rc = pthread_attr_init(&attr);
	if(rc != 0)
	{
		printf("pthread_attr_init() returns error: %s\n",strerror(errno));
		return -1;
	}
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(rc != 0)
	{
		printf("pthread_attr_setdetachstate() returns error: %s\n",strerror(errno));
		return -1;
	}
	rc = pthread_create(&thread, &attr,fp, data);
	if(rc != 0)
	{
		printf("pthread_create() returns error: %s\n",strerror(errno));
		return -1;
	}
	return 0;
}

void macPacketPrint(const unsigned char * buf, const int num)
{
	int i;
	if(num < 60){
		printf("packet size is %d, is too small, the smallest size is 64\n",num);
		return;
	}

	for(i = 0; i < 14; i++){
		printf("%02X ", buf[i]);
		if ( (i+1)% 6 == 0 )
			printf("   ");
	}
	printf("\n");

	for(;i < num; i++){
		printf("%02X ", buf[i]);
		if ( ((i+1-14)% 8 == 0) && ((i+1-14)% prChunkSize != 0))
				printf("   ");

		if ((i+1-14)% prChunkSize == 0)
			printf("\n");
	}
	if(num%prChunkSize)
		printf("\n");
	for(i =0; i <prChunkSize; i++)
		printf("===");
	printf("\n");
	return;

}

void batchedPacketPrint(const unsigned char * buf, const int totalSize, const int perSize)
{
	int i, j, base;

	if(totalSize % perSize){
		printf("totalSize should be integral multiple of perSize\n");
		return;
	}
	for(j = 0; j< totalSize/perSize; j++){
		base = j * perSize;
		//print mac header 14 bytes
		for(i = base; i < 14 + base; i++){
			printf("%02X ", buf[i]);
			if ( (i - base +1)% 6 == 0 ){
				printf("   ""   ");
				printf("   ");
			}
		}
		printf("\n");

		//print the rest
		for(;i < base + perSize; i++){
			printf("%02X ", buf[i]);
			if ( ((i - base -14 + 1)% 8 == 0) && ((i - base - 14 + 1)% prChunkSize != 0))
					printf("   ");

			if ((i - base +1-14)% prChunkSize == 0)
				printf("\n");
		}
		printf("\n\n");
	}
	for(i =0; i <prChunkSize + prChunkSize/8 - 1; i++)
		printf("===");
	printf("\n");
	return;

}

void hexPrint(const unsigned char *buf, const int num)
{
    int i;
    for(i = 0; i < num; i++)
    {
        printf("%02X ", buf[i]);
        if ( ((i+1)% 8 == 0) && ((i+1)% prChunkSize != 0))
                    printf("   ");

        if ((i+1)% prChunkSize == 0)
            printf("\n");
    }
    if(num%prChunkSize)
    	printf("\n");
    for(i =0; i <prChunkSize; i++)
    	printf("===");
    printf("\n");
    return;
}

void fhexPrint(const FILE * file, const unsigned char * buf, const int num){
	int i;
	for(i = 0; i < num; i++)
	{
		fprintf(file,"%02X ", buf[i]);
		if ((i+1)% prChunkSize == 0)
			fprintf(file,"\n");
	}
	fprintf(file,"\n\n");
	return;
}




// this function should be used only once and used very carefully, as it resets all previous bookmarkings.
int initMemorySync (MemorySync* memInfo, unsigned long int totalMemory)
{
	// assert if bad MemorySync pointer memInfo
	if (!memInfo)
	{
		//error
		return MEM_FAILURE;
	}

	pthread_mutex_lock(&memInfo->iLock);
	memInfo->iAvailableMemory = totalMemory;
	pthread_mutex_unlock(&memInfo->iLock);
	return MEM_SUCCESS;
}


int ReserveAvailable(MemorySync *memInfo, unsigned int requested)
{
	// assert if bad MemorySync pointer memInfo
	if (!memInfo)
	{
		//error
		return MEM_FAILURE;
	}

	if (requested > 0)
	{
		pthread_mutex_lock(&memInfo->iLock);
		if (memInfo->iAvailableMemory >= requested)
		{
			memInfo->iAvailableMemory -= requested;
			requested = 0; // or we can break here, if we need while (1) implementation.
			// log_verbose("res %d ",memInfo->iAvailableMemory);
			//       break ;
		}
		pthread_mutex_unlock(&memInfo->iLock);
	}
	if(requested ==0)
		return MEM_SUCCESS;
	else
		return MEM_NOTAVLB;
}

int FreeAvailable(MemorySync *memInfo, unsigned int completed)
{
	// assert if bad MemorySync pointer memInfo
	if (!memInfo)
	{
		//error
		return MEM_FAILURE;
	}

	pthread_mutex_lock(&memInfo->iLock);
	memInfo->iAvailableMemory += completed;


	pthread_mutex_unlock(&memInfo->iLock);

	return MEM_SUCCESS;
}

/*
int UpdateAvailable(MemorySync *memInfo, int engine)
{
	// assert if bad MemorySync pointer memInfo
	if (!memInfo)
	{
		//error
		return MEM_FAILURE;
	}
	if(engine == 0)
	{
		pthread_mutex_lock(&memInfo->iLock);
		RxBufferSize0= memInfo->iAvailableMemory;
		pthread_mutex_unlock(&memInfo->iLock);
	}
	else if(engine == 1)
	{
		pthread_mutex_lock(&memInfo->iLock);
		RxBufferSize1= memInfo->iAvailableMemory;
		pthread_mutex_unlock(&memInfo->iLock);
	}
	else if(engine == 2)
	{
		pthread_mutex_lock(&memInfo->iLock);
		RxBufferSize2= memInfo->iAvailableMemory;
		pthread_mutex_unlock(&memInfo->iLock);
	}
	else if(engine == 3)
	{
		pthread_mutex_lock(&memInfo->iLock);
		RxBufferSize3= memInfo->iAvailableMemory;
		pthread_mutex_unlock(&memInfo->iLock);
	}

	else
	{
		printf("##wrong engine ##\n");
	}
	return 0;
}
*/
