
plugin_o = plugin.o  IDL_SeqRequest.dsc.o

INSTALL_HEADER = *.h 
INSTALL_APP =  libseqmysql_output.so

all	: $(INSTALL_APP) 
	
libseqmysql_output.so : $(plugin_o)
	g++ -shared -o $@  $(plugin_o) -L. -L$(HOME)/lib -luhp_mysql -luhp_util -luhp_api
	

.cpp.o	:
	g++ -g -fPIC -O1 -Wno-write-strings  -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include
.c.o	:
	gcc -std=gnu99 -g -fPIC -O1  -Wno-write-strings  -o $@ -c $< -I. -I/usr/include -I$(HOME)/include

clean	:
	rm -f *.so *.o

install :
	make
	mkdir -p $(HOME)/lib
	mkdir -p $(HOME)/include
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
