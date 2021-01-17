
util_o = list.o LOGS.o LOG.o uhp_util.o  fasterhttp.o fasterjson.o threadpool.o autolock.o \

INSTALL_HEADER = *.h 
INSTALL_APP =  libuhp_util.so

all	:	   $(INSTALL_APP)

libuhp_util.so : $(util_o)
	g++ -shared -o $@  $(util_o) -L$(HOME)/lib -lpthread -lssl

.cpp.o	:
	g++ -g -fPIC -O2 -Wall -Werror -Wno-write-strings   -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include
.c.o	:
	gcc -std=gnu99 -g -fPIC -O2  -Wall -Werror -Wno-write-strings   -o $@ -c $< -I. -I/usr/include -I$(HOME)/include

clean	:
	rm -f *.so *.o

install :
	make
	mkdir -p  $(HOME)/lib
	mkdir -p  $(HOME)/include
	for i in $(INSTALL_APP);\
	do \
		echo "rm -f $$i";\
		rm -f $(HOME)/lib/$$i;\
		echo "cp -f $$i";\
		cp -f $$i $(HOME)/lib;\
	done
	for i in $(INSTALL_HEADER);\
	do \
		echo "cp -f $$i";\
		cp -f $$i $(HOME)/include/;\
	done
