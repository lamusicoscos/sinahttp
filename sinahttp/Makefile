LIBEVENT ?= /usr/local
TARGET ?= /usr/local

CFLAGS = -I. -I$(LIBEVENT)/include -Wall -g
LIBS = -L. -L$(LIBEVENT)/lib -levent -lm

AR = ar
AR_FLAGS = rc
RANLIB = ranlib

libsinahttp.a: sinahttp.o async_sinahttp.o timer.o log.o util.o stat.o request.o options.o
	/bin/rm -f $@
	$(AR) $(AR_FLAGS) $@ $^
	$(RANLIB) $@

testserver: testserver.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lsinahttp

all: libsinahttp.a testserver

install:
	/usr/bin/install -d $(TARGET)/lib/
	/usr/bin/install -d $(TARGET)/include/sinahttp/
	/usr/bin/install libsinahttp.a $(TARGET)/lib/
	/usr/bin/install sinahttp.h $(TARGET)/include/sinahttp/
	/usr/bin/install sinahttp.h $(TARGET)/include/sinahttp/
	/usr/bin/install queue.h $(TARGET)/include/sinahttp/
	/usr/bin/install uthash.h $(TARGET)/include/sinahttp/
	/usr/bin/install utlist.h $(TARGET)/include/sinahttp/
	/usr/bin/install utstring.h $(TARGET)/include/sinahttp/
	/usr/bin/install utarray.h $(TARGET)/include/sinahttp/
	/usr/bin/install options.h $(TARGET)/include/sinahttp/

clean:
	rm -rf *.a *.o testserver *.dSYM
