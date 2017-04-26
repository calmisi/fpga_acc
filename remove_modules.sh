#!/bin/sh
DMA_MODULE_NAME="xdma_v7"
RAWMODULE="xrawdata0"
STATSFILE="xdma_stat"

if [ -d /sys/module/$DMA_MODULE_NAME ]; then
	if [ -d /sys/module/$RAWMODULE ]; then
		sudo make remove
	else
		sudo rmmod $DMA_MODULE_NAME
	fi
fi
if [ -c /dev/$STATSFILE ]; then
	sudo rm -rf /dev/$STATSFILE
fi
