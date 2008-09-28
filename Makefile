# $Id: Makefile 58 2007-10-22 20:40:31Z d3urk $
EXTRACFLAGS=-Wall -O2
CC=gcc $(EXTRACFLAGS)
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = cmd.o crc.o cvar.o forward.o forward_pending.o info.o main.o mdfour.o \
	msg.o net_utils.o parse.o qw.o source.o source_cmds.o sys.o build.o token.o httpsv.o httpsv_generate.o \
	cl_cmds.o fs.o bann.o

qtv: $(OBJS) qtv.h qconst.h
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@.bin

qtv.exe: *.c *.h
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv qtv qtv.exe

clean:
	rm -rf qtv.bin qtv.exe qtv.db *.o
