PANDA		:= /usr/proj/panda/panda2.0/latest_greatest
ARCH		:= myrinet/optimized

SRC_DIR		:= $(PANDA)/tst/src/mp

# Additional local flags
CPPFLAGS	+= -DRELIABLE_UNICAST -DNDEBUG
CPPFLAGS	+= -DPOLL_ON_WAIT
CFLAGS		+= -g -O2 -finline-functions -mv8
LIBS		+=

.PHONY: all
all:		test1 test2 test3 test4 test5 test6 test7 test8

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

test6:		test6.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test7:		test7.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test8:		test8.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

.PHONY: clean
clean:	
		rm -f *.o test? Makefile.bak core gmon.out


include $(PANDA)/include/appl.mk
