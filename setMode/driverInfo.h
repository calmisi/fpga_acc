#include "xpmon_be.h"

#ifndef _Included_DriverInfoGen
#define _Included_DriverInfoGen
#ifdef __cplusplus
extern "C" {
#endif

int driverInfo_init();
int driverInfo_flush(int __fd);
int driverInfo_startTest(int __fd, int engine, int testmode, int maxsize);
int driverInfo_stopTest(int __fd, int engine, int testmode, int maxsize);


int driverInfo_LedStats(int __fd, LedStats * ledStats);
int driverInfo_get_EngineState(int __fd, int engineNo, int tmp[MAX_ENGS][14]);
int driverInfo_get_DMAStats(int __fd, float tmp[MAX_ENGS][4]);
int driverInfo_get_TRNStats(int __fd);

#ifdef __cplusplus
}
#endif
#endif

