PANDA		:=
ARCH		:= parix2.1-PowerPC
VERSION		:=

SRC_DIR		:= $(PANDA)/tst/src/system/comm

# Additional local flags
CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+=

all:		fucast_pingpong ucast_pingpong

fucast_pingpong:	fucast_pingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

ucast_pingpong:	ucast_pingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:          
		rm -f Makefile.bak panda_adm *.o *.d lat core

include $(PANDA)/include/appl.mk
