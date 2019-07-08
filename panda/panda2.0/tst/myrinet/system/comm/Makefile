PANDA_BASE	:= /usr/proj/panda/panda2.0
# PANDA		:= $(PANDA_BASE)
PANDA		:= $(PANDA_BASE)/latest_greatest
# ARCH		:= myrinet/dumb/reliable
# ARCH		:= myrinet/dumb/reliable/optimized
ARCH		:= myrinet/dumb/rcve_cont/reliable
# ARCH		:= myrinet
# ARCH		:= myrinet/reliable
# ARCH		:= myrinet/optimized
# ARCH		:= myrinet/reliable/optimized
# ARCH		:= myrinet/user_threads
# ARCH		:= myrinet/user_threads/optimized
# ARCH		:= myrinet/user_threads/reliable
# ARCH		:= myrinet/user_threads/reliable/optimized
# ARCH		:= myrinet/user_threads/reliable/optimized/flat
# ARCH		:= myrinet/preemptive_threads/reliable
# ARCH		:= myrinet/preemptive_threads/reliable/optimized
# ARCH		:= myrinet/preemptive_threads/optimized
# ARCH		:= myrinet/optimized/panda_pool
# ARCH		:= myrinet/profiling
# ARCH		:= myrinet/flat

FM_LIB		:= /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
# FM_LIB		:= /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libuserFM.a

# SRC_DIR		:= .
SRC_DIR		:= $(PANDA)/tst/src/system/comm
# SRC_DIR		:= $(PANDA)/tst/src/system/comm/no_fragment

CPPFLAGS	+= -I$(PANDA_BASE)/src/system/myrinet

CPPFLAGS	+= -DMYRINET
# CPPFLAGS	+= -DOS_FRAGMENTS
# CPPFLAGS	+= -DRELIABLE_UNICAST
CPPFLAGS	+= -DORDERED_MULTICAST
CPPFLAGS	+= -DRELIABLE_MULTICAST
# CPPFLAGS	+= -DRELIABLE_UNICAST

# CPPFLAGS	+= -DNDEBUG
# CPPFLAGS	+= -DDO_TIMINGS

CFLAGS		+= -g -ansi
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes
# CFLAGS		+= -pg

# CFLAGS		+= -O2 -finline-functions
# CFLAGS		+= -mflat

LIBS		+= -lm

default:	mpingpong

mcast:		mcast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ mcast.o $(LIBS)

mpng:		mpng.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ mpng.o $(LIBS)

mpingpong:	mpingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ mpingpong.o $(LIBS)

fucast_pingpong:	fucast_pingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fucast_pingpong.o $(LIBS)

fpingpong:	fpingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fpingpong.o $(LIBS)

fucast:		fucast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fucast.o $(LIBS)

fmcast:		fmcast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ fmcast.o $(LIBS)

upingpong:	upingpong.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ upingpong.o $(LIBS)

unicast:	unicast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ unicast.o $(LIBS)

ucast:		ucast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ ucast.o $(LIBS)

pair_ucast:	pair_ucast.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ pair_ucast.o $(LIBS)

msgbuild:	msgbuild.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

throughput:	throughput.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f *.o *.d core


include $(PANDA)/include/appl.mk
