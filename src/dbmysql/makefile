
dbmysql_o =  dbsqlca.o mysqldb.o  dbutil.o \


test_o = test.o

INSTALL_HEADER = *.h 
INSTALL_APP =  libuhp_mysql.so test

all	: $(INSTALL_APP)	
	
libuhp_mysql.so : $(dbmysql_o)
	g++ -shared -o $@  $(dbmysql_o) -L$(HOME)/lib -luhp_util -L/usr/lib64/mysql -lmysqlclient

test	: $(test_o)
	g++ -o $@  $(test_o) -L. -L$(HOME)/lib -luhp_mysql -luhp_util -lssl -lpthread

.cpp.o	:
	g++ -g -fPIC -O2 -Wno-write-strings -o $@ -c $< -I. -I/usr/include/mysql -I/usr/include -I$(HOME)/include
.c.o	:
	gcc -std=gnu99 -g -fPIC -O2 -Wno-write-strings  -o $@ -c $< -I. -I/usr/include -I$(HOME)/include

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
