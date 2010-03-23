SHELL = /bin/sh

prefix=/usr
exec_prefix=${prefix}
datadir=${prefix}/share
sysconfdir=${prefix}/etc
sbindir=${exec_prefix}/sbin
lsbindir=${sbindir}
mandir=${prefix}/share/man

includedir=${prefix}/include
libdir=${exec_prefix}/lib
bindir=${exec_prefix}/bin

srcdir=
top_srcdir=
BUILDTOP=
CC=gcc -m32
INSTALL=/usr/bin/install -c

IRFLAGS=-DINTERREALM
CPPFLAGS=-I/usr/athena/include
CFLAGS=-g -O2
ALL_CFLAGS=${CFLAGS} ${IRFLAGS} ${CPPFLAGS}
LDFLAGS=-L${BUILDTOP}/lib 
#LIBS=-lreadline -L/usr/athena/lib -Wl,-R /usr/athena/lib -lzephyr -lkrb4 -lkrb5 -lcrypto -lcrypt -lresolv -lcom_err -ldl 
LIBS=-lreadline -L/usr/athena/lib -lzephyr -lkrb4 -lkrb5 -lcrypto -lcrypt -lresolv -lcom_err -ldl 

OBJS= ZCkAuth.o lread.o

all: zsend

zsend.o: zsend.c

zsend: zsend.o lread.o lread.h ZCkAuth.o
	${CC} ${LDFLAGS} -o $@ ZCkAuth.o lread.o zsend.o ${LIBS}

.c.o:
	${CC} -c ${ALL_CFLAGS} $<

check:

install: zsend
	${INSTALL} -m 755 -s zsend ../../bin

clean:
	rm -f *.o zsend

.PHONY: all check install clean

