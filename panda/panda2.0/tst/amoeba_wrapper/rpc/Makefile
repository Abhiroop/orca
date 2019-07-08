PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_wrapper

SRC_DIR		:= $(PANDA)/tst/src/rpc

# Additional local flags
CPPFLAGS	+= -DNDEBUG
CFLAGS		+= -O2 -finline-functions -mv8
LIBS		+=

all:		test1 test2 test3 test4

test1:		test1.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test2:		test2.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test3:		test3.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test4:		test4.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:	
		rm -f *.o test? Makefile.bak core gmon.out

include $(PANDA)/include/appl.mk
