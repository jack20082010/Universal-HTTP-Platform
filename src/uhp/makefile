server_o = main.o env.o  monitor.o worker.o app.o client.o  config.o comm.o \
	IDL_httpserver_conf.dsc.o IDL_httpserver_conf.dsc.LOG.o \


api_o = uhp_api.o \

INSTALL_HEADER = uhp_api.h uhp_ver.h
INSTALL_APP =  libuhp_api.so

all	: $(INSTALL_APP) uhp

libuhp_api.so : $(api_o)
	gcc -shared -o $@  $(api_o)
	
	
uhp	:	$(server_o)
	gcc -o $@ $(server_o) -L. -L$(HOME)/lib -luhp_api  -luhp_util -lpthread -ldl -lz -lm -lssl 
#	ctags -R .

.cpp.o	:
	g++ -g -fPIC -O2 -Wno-write-strings  -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include
.c.o	:
	gcc -std=gnu99 -g -fPIC -O2  -Wno-write-strings  -o $@ -c $< -I. -I/usr/include -I$(HOME)/include -D_GNU_SOURCE

clean	:
	rm -f uhp *.so *.o

install :
	make
	for i in $(INSTALL_APP);\
	do \
		echo "rm -f $$i";\
		rm -f $(HOME)/lib/$$i;\
		echo "cp -f $$i";\
		cp -f $$i $(HOME)/lib;\
	done

	rm -f $(HOME)/bin/uhp
	cp -f uhp $(HOME)/bin/

	for i in $(INSTALL_HEADER);\
	do \
		echo "cp -f $$i";\
		cp -f $$i $(HOME)/include/;\
	done
