#!/bin/sh

OSNAME=`uname -a|awk '{print $1}'`
VERSION=`cat $HOME/include/uhp_sdk.h | grep SDK_VERSION| tr '"' ' ' | awk '{print $3}'`
echo $VERSION
TARGZFILE="uhp-${OSNAME}-${VERSION}-sdk-dev.tar.gz"
#echo $TARGZFILE
cd
tar cvzfh $TARGZFILE \
	lib/libuhp_util.so \
	lib/libuhp_sdk.so \
	include/uhp_sdk.h \
	--exclude=.git \

echo "Packing $TARGZFILE OK!"


