#!/bin/sh
compilation_error=1
module_insertion_error=2
compilation_clean_error=3

/bin/sh remove_modules.sh
#cd driver
make DRIVER_MODE=RAWETHERNET clean
if [ "$?" != "0" ]; then
	echo "Error in cleaning Performance Driver"
	exit $compilation_clean_error;
fi
make DRIVER_MODE=RAWETHERNET
if [ "$?" != "0" ]; then
	echo "Error in compiling Performance Driver"
	exit $compilation_error;
fi
sudo make DRIVER_MODE=RAWETHERNET insert 
if [ "$?" != "0" ]; then
	echo "Error in Inserting Performance Driver"
	exit $module_insertion_error;
fi
