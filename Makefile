#
#	Makefile for Xilinx Virtex-7 FPGA XT Connectivity Targeted Reference Design
#

export ROOTDIR=$(shell pwd)
include $(ROOTDIR)/include.mk

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
export OS_TYPE=64
endif
ifeq ($(LBITS),32)
export OS_TYPE=32
endif


all::
ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),PERFORMANCE ))
	    @$(call compile_performance_driver)
endif

ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),RAWETHERNET ))
	    @$(call compile_rawethernet_driver)
endif
		@echo "***** Driver Compiled *****"

clean::
ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),PERFORMANCE ))
	    @$(call clean_performance_driver)
endif

ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE), RAWETHERNET ))
	    @$(call clean_rawethernet_driver)
endif
		@echo "***** Driver Cleaned *****"

insert:: 
ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),PERFORMANCE ))
	    @$(call insert_performance_driver)
endif

ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),RAWETHERNET ))
	    @$(call insert_rawethernet_driver)
endif
		@echo "***** Driver Loaded *****"

remove::
ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),PERFORMANCE))
	    @$(call remove_performance_driver)
endif

ifeq ($(DRIVER_MODE),$(filter $(DRIVER_MODE),RAWETHERNET ))
	    @$(call remove_rawethernet_driver)
endif
		@echo "***** Driver Unloaded *****"

