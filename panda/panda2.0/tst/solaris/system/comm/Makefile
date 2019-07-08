PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= solaris/optimized

SRC_DIR		:= $(PANDA)/tst/src/system/comm

# Additional local flags
CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+=

fucast_pingpong:	fucast_pingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

ucast_pingpong:	ucast_pingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:          
		rm -f Makefile.bak panda_adm *.o *.d core

include $(PANDA)/include/appl.mk
