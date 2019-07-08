PANDA_BASE	:= /usr/proj/panda/panda2.0
# PANDA		:= $(PANDA_BASE)
PANDA		:= $(PANDA_BASE)/latest_greatest
# ARCH		:= myrinet/dumb/reliable
# ARCH		:= myrinet/dumb/reliable/optimized
ARCH		:= myrinet/dumb/rcve_cont/reliable
# ARCH		:= myrinet/dumb_msg/reliable
# ARCH		:= myrinet/dumb_msg/reliable/optimized
# ARCH		:= myrinet
# ARCH		:= myrinet/reliable
# ARCH		:= myrinet/optimized
# ARCH		:= myrinet/user_threads
# ARCH		:= myrinet/user_threads/reliable
# ARCH		:= myrinet/user_threads/reliable/optimized
# ARCH		:= myrinet/user_threads/reliable/optimized/flat

# SRC_DIR		:= .
SRC_DIR		:= $(PANDA_BASE)/tst/src/group

CPPFLAGS	+= -DMYRINET
# CPPFLAGS	+= -DALLOW_MCAST_FROM_UPCALL
CPPFLAGS	+= -DSTATISTICS
CPPFLAGS	+= -DVERBOSE
CPPFLAGS	+= -DPOLL_ON_WAIT

# CPPFLAGS	+= -DNDEBUG

CFLAGS		+= -g -ansi -pedantic
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes

# CFLAGS		+= -O2 -finline-functions
# CFLAGS		+= -mflat

LIBS		+= -lm

default:	grp_test gpingpong gpng

all_lat:	all_lat.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ all_lat.o $(LIBS)

all_lat_upcall:	all_lat_upcall.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ all_lat_upcall.o $(LIBS)

all_throughput:	all_throughput.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ all_throughput.o $(LIBS)

grp_test:	grp_test.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ grp_test.o $(LIBS)

all_latency:	all_latency.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ all_latency.o $(LIBS)

gpingpong:	gpingpong.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ gpingpong.o $(LIBS)

gpng:		gpng.o -lpanda /usr/proj/orca/Networks/FM/myri-fm-1.1-am/lib.am/libFM.a
		$(LD) $(LDFLAGS) -o $@ gpng.o $(LIBS)

.PHONY: clean
clean:
		-rm -f *.o *.d core


include $(PANDA)/include/appl.mk
