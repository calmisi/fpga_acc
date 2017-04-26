export ROOTDIR=$(shell pwd)

OBJS = setMode.o driverInfo.o

CC = gcc
CFLAGS = -DPM_SUPPORT -DK7_TRD
LFLAGS = -lpthread

all : $(OBJS)
	gcc $(OBJS) $(CFLAGS) $(LFLAGS) -o setMode

setFpgaMode.o : setMode.c driverInfo.h

driverInfo.o : driverInfo.h xpmon_be.h
	$(CC) -c -DPM_SUPPORT driverInfo.c 

clean: 
	rm -rf setMode $(OBJS) *.o
