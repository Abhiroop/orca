PANDA		:= /usr/proj/panda/panda2.0
VERSION		:= latest_greatest
ARCH		:= solaris/optimized

SRC_DIR		:= $(PANDA)/tst/src/rpc

# Additional local flags
CPPFLAGS	+=
CFLAGS		+= -g
LIBS		+=

all:		test1 test2 test3 test4 test5

test1:		test1.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test2:		test2.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test3:		test3.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test4:		test4.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test5:		test5.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test_n:		test_n.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:	
		rm -f *.o test? Makefile.bak core gmon.out

include $(PANDA)/include/appl.mk
