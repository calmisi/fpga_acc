OBJS = sendPacket.c tools.o xpmon_be.h calm_debug.h

CFLAGS = -DMMap -DDEBUG_VERBOSE
CFLAGS += -D_DEBUG
LFLAGS = -lpthread

all: clean $(OBJS)
	gcc -o sendPacket $(OBJS) $(CFLAGS) $(LFLAGS)

tools.o: tools.h tools.c
	gcc -c $^ $(LFLAGS)

clean:
	rm -rf sendPacket *.o
