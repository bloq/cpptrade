#!/bin/bash

CWD=`pwd`
PID_FN="$CWD/obsrv.pid"
CFG_FN="./test-config-obsrv.json"

#
# Daemon startup
#

# PID file cannot be present at start of tests
if [ -f $PID_FN ]
then
	echo Daemon PID file already exists.
	exit 1
fi

# Start daemon in background
./obsrv -c $CFG_FN -p $PID_FN --daemon
sleep 1

# PID file must exist
if [ ! -f $PID_FN ]
then
	echo Daemon not started.
	exit 1
fi

# Ping process to make sure it exists
kill -0 `cat $PID_FN`
if [ $? -ne 0 ]
then
	echo "Daemon cannot be pinged with kill(1)"
	kill %1
	rm -f $PID_FN
	exit 1
fi

RETVAL=0

#./obsrv-test-client
#if [ $? -ne 0 ]
#then
#	echo "Orad test client failed."
#	RETVAL=1
#fi

#
# Daemon shutdown
#

if [ ! -f $PID_FN ]
then
	echo Daemon PID file is missing.
	exit 1
fi

kill `cat $PID_FN`
if [ $? -ne 0 ]
then
	echo "Daemon cannot be reached via kill(1)"
	exit 1
fi

sleep 1

if [ -f $PID_FN ]
then
	echo Daemon PID file remains.
	exit 1
fi

exit $RETVAL

