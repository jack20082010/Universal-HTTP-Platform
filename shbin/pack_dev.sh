#!/bin/sh

OSNAME=`uname -a|awk '{print $1}'`
VERSION=`cat $HOME/include/httpserver_ver.h | tr '"' ' ' | grep SERVER_VERSION | awk '{print $3}'`
TARGZFILE="hs-${OSNAME}-${VERSION}-dev.tar.gz"

cd
tar cvzfh $TARGZFILE bin/httpserver \
	lib/*.so \
	shbin \
	include/httpserver_api.h \
	--exclude=.git \

echo "Packing $TARGZFILE OK!"


