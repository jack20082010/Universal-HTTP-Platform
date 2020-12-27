#!/bin/sh

OSNAME=`uname -a|awk '{print $1}'`
VERSION=`cat $HOME/include/httpserver_ver.h | tr '"' ' ' | grep SERVER_VERSION | awk '{print $3}'`
TARGZFILE="hs-${OSNAME}-${VERSION}-bin.tar.gz"

cd
tar cvzfh $TARGZFILE bin/httpserver \
	lib/*.so \
	shbin \
	--exclude=.git \

echo "Packing $TARGZFILE OK!"


