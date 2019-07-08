PANDA_BASE	:= /usr/proj/panda/panda2.0
# PANDA		:= $(PANDA_BASE)
PANDA		:= $(PANDA_BASE)/latest_greatest
# FLAVOUR		:= preemptive_threads/optimized
# FLAVOUR		:= optimized
# FLAVOUR		:= user_threads
# FLAVOUR		:= amoeba_threads
FLAVOUR		:= user_threads/optimized
ARCH		:= fast_ethernet/$(FLAVOUR)

FM_LIB		:= /usr/proj/orca/Networks/FastEthernet/lib/usched/libFM.a

# SRC_DIR		:= .
SRC_DIR		:= $(PANDA)/tst/src/system/comm

CPPFLAGS	+= -I$(PANDA_BASE)/src/system/myrinet

# CPPFLAGS	+= -DDO_TIMINGS
CPPFLAGS	+= -DNDEBUG

CPPFLAGS	+= -DPOLL_ON_WAIT

CFLAGS		+= -g -ansi
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes

CFLAGS		+= -O2 -finline-functions
CFLAGS		+= -mflat
# CFLAGS		+= -pg

LIBS		+= -lm

unicast:	unicast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ unicast.o $(LIBS)

ucast:		ucast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ ucast.o $(LIBS)

fucast:		fucast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fucast.o $(LIBS)

ucast_pingpong:	ucast_pingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ ucast_pingpong.o $(LIBS)

fucast_pingpong:	fucast_pingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fucast_pingpong.o $(LIBS)

fpingpong:	fpingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fpingpong.o $(LIBS)

msgbuild:	msgbuild.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

throughput:	throughput.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f *.o *.d core


include $(PANDA)/include/appl.mk
