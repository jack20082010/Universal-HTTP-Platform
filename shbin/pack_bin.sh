#!/bin/sh

OSNAME=`uname -a|awk '{print $1}'`
VERSION=`cat $HOME/include/uhp_ver.h | tr '"' ' ' | grep SERVER_VERSION | awk '{print $3}'`
TARGZFILE="hs-${OSNAME}-${VERSION}-bin.tar.gz"

cd
tar cvzfh $TARGZFILE bin/uhp \
	lib/*.so \
	shbin \
	--exclude=.git \

echo "Packing $TARGZFILE OK!"


