#!/bin/bash

usage()
{
	echo "USAGE : uhp.sh [ status | start | stop | kill | restart | reload | kill_shm ]"
}

if [ $# -eq 0 ] ; then
	usage
	exit 9
fi

case $1 in
	status)
		ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3=="1")print $0}'
		ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3!="1")print $0}'
		;;
	start)
		if [ x"$IBP_LOGDIR" != x"" ] ; then
			if [ ! -d $IBP_LOGDIR ] ; then
				mkdir -p $IBP_LOGDIR
			fi
		else
			if [ ! -d $HOME/log ] ; then
				mkdir -p $HOME/log 
			fi
		fi
		
		PID=`ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" != x"" ] ; then
			echo "*** ERROR : uhp existed"
			exit 1
		fi
		uhp -f uhp.conf -a start
		NRET=$?
		if [ $NRET -ne 0 ] ; then
			echo "*** ERROR : uhp start error[$NRET]"
			exit 1
		fi
		while [ 1 ] ; do
			sleep 1
			PID=`ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3!="1")print $2}'`
			if [ x"$PID" != x"" ] ; then
				break
			fi
		done
		echo "uhp start ok"
		uhp.sh status
		;;
	stop)
		uhp.sh status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		PID=`ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" = x"" ] ; then
			echo "*** ERROR : uhp not existed"
			exit 1
		fi
		kill $PID

		TIMELOOP=0
		while [ 1 ] ; do

			sleep 1
			TIMELOOP=`expr $TIMELOOP + 1`
			echo "cost time $TIMELOOP $PID wait closing" ;
			PID=`ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3=="1")print $2}'`
			if [ x"$PID" = x"" ] ; then
				break
			fi
		done
		echo "uhp end ok"
		;;
	kill)
		uhp.sh status
		killall -9 uhp
		;;
	restart)
		uhp.sh stop
		sleep 1
		uhp.sh start
		;;
	reload)
		uhp.sh status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		PID=`ps -f -u $USER | grep "uhp -f uhp.conf" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" = x"" ] ; then
			echo "*** ERROR : uhp not existed"
			exit 1
		fi
		kill -USR1 $PID
		;;
	kill_shm)
		for i in `ipcs -m | grep -w $USER | awk '{ if($1=="0x00000000" && $6=="0") print $2 }'`
		do
			echo "ipcrm -m $i"
			ipcrm -m $i
		done
		;;
	*)
		usage
		;;
esac

