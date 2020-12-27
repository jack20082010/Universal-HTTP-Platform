#!/bin/sh

OSNAME=`uname -a|awk '{print $1}'`
VERSION=`cat $HOME/include/nsproxy/nsproxy_sdk.h | grep NSPROXY_SDK_VERISON| tr '"' ' ' | awk '{print $3}'`
#echo $VERSION
TARGZFILE="ns-${OSNAME}-${VERSION}-sdk-dev.tar.gz"
#echo $TARGZFILE
cd
tar cvzfh $TARGZFILE \
	bin.tools/nspsdk_ver \
	lib/libibp_util.so \
	lib/libibp_idl.so \
	lib/libhzb_log.so \
	lib/libnsproxy_sdk.so \
	include/nsproxy \
	--exclude=.git \

echo "Packing $TARGZFILE OK!"


