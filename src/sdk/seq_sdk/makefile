
plugin_o = seq_sdk.o IDL_uhp_response.dsc.o

test_o = main.o

INSTALL_HEADER = seq_sdk.h 
INSTALL_APP =  libseq_uhp.so

all	: $(INSTALL_APP) test
	
libseq_uhp.so : $(plugin_o)
	g++ -shared -o $@  $(plugin_o) -L. -L$(HOME)/lib  -luhp_util 
	
test : $(test_o)
	g++  -o $@  $(test_o) -L. -L$(HOME)/lib  -lseq_uhp -lpthread -luhp_mysql

.cpp.o	:
	g++ -g -fPIC -O2 -Wall -Werror -Wno-write-strings  -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include

.c.o	:
	gcc -std=gnu99 -g -fPIC -O2 -Wall -Werror -Wno-write-strings   -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include

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
