PANDA_BASE	:= /usr/proj/panda/panda2.0
PANDA		:= $(PANDA_BASE)/latest_greatest
# FLAVOUR		:=
# FLAVOUR		:= optimized
# FLAVOUR		:= user_threads
FLAVOUR		:= user_threads/optimized
# FLAVOUR		:= preemptive_threads
# FLAVOUR		:= preemptive_threads/optimized
# FLAVOUR		:= amoeba_threads
ARCH		:= fast_ethernet/$(FLAVOUR)

SRC_DIR		:= $(PANDA_BASE)/tst/src/rpc
# SRC_DIR		:= .

# Additional local cpp flags
CPPFLAGS	+= -DPOLL_ON_WAIT

CPPFLAGS	+= -DNDEBUG


# Additional local cc flags
CFLAGS		+= -mflat
CFLAGS		+= -Wall -g -mv8

CFLAGS		+= -O2 -finline-functions


LIBS		+=

# all:		test1 test2 test3 test4 test_n
all:		tst_n

rpc:		rpc.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

tst_n:		tst_n.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test_n:		test_n.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

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

clean:	
		rm -f *.o test? Makefile.bak core gmon.out

include $(PANDA)/include/appl.mk
