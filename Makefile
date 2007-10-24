# $Id: Makefile 58 2007-10-22 20:40:31Z d3urk $
EXTRACFLAGS=-Wall -O2
CC=gcc $(EXTRACFLAGS)
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = cmd.o crc.o cvar.o forward.o forward_pending.o info.o main.o mdfour.o \
	msg.o net_utils.o parse.o qw.o source.o source_cmds.o sys.o build.o token.o httpsv.o httpsv_generate.o \
	cl_cmds.o fs.o

eztv: $(OBJS) qtv.h qconst.h
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

eztv.exe: *.c *.h
	$(MAKE) eztv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv eztv eztv.exe

clean:
	rm -rf eztv eztv.exe eztv.db *.o
