#!/bin/sh
compilation_clean_error=3

#/bin/sh remove_modules.sh
make  clean
if [ "$?" != "0" ]; then
	echo "Error in cleaning Performance Driver"
	exit $compilation_clean_error;
fi

