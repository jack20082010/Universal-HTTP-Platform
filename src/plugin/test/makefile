
plugin_o = plugin.o

all	:	  libplugin.so 
	
libplugin.so : $(plugin_o)
	g++ -shared -o $@  $(plugin_o) -L. -L$(HOME)/lib -luhp_mysql -luhp_util -luhp_api
	

.cpp.o	:
	g++ -g -fPIC -O2 -Wno-write-strings  -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include
.c.o	:
	gcc -std=gnu99 -g -fPIC -O2  -Wno-write-strings  -o $@ -c $< -I. -I/usr/include -I$(HOME)/include

clean	:
	rm -f *.so *.o

install :
	make
#	if [ -f $(HOME)/lib/libdbmysql.so ] ; then \
#		rm -f $(HOME)/lib/libdbmysql.so \
#	fi
	cp -f libplugin.so $(HOME)/lib/
#	
#	if [ -f $(HOME)/lib/libhttpserver_api.so ] ; then \
#		rm -f $(HOME)/lib/libhttpserver_api.so \
#	fi
#	
#	cp -f shbin/* $(HOME)/shbin/
