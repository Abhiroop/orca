PANDA_BASE	:= /usr/proj/panda/panda2.0
# PANDA		:= $(PANDA_BASE)
PANDA		:= $(PANDA_BASE)/latest_greatest
# FLAVOUR		:= amoeba_threads
# FLAVOUR		:= user_threads
# FLAVOUR		:= user_threads/optimized
# FLAVOUR		:= preemptive_threads
# FLAVOUR		:= preemptive_threads/optimized
# FLAVOUR		:=
FLAVOUR		:= optimized
ARCH		:= fast_ethernet/$(FLAVOUR)

# SRC_DIR		:= .
SRC_DIR		:= $(PANDA_BASE)/tst/src/group

# CPPFLAGS	+= -DALLOW_MCAST_FROM_UPCALL
CPPFLAGS	+= -DNDEBUG
CPPFLAGS	+= -DSTATISTICS
CPPFLAGS	+= -DVERBOSE
CPPFLAGS	+= -DPOLL_ON_WAIT

CFLAGS		+= -g -ansi -pedantic
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes
CFLAGS		+= -O2 -finline-functions
CFLAGS		+= -mflat

LIBS		+= -lm

default:	grp_test gpingpong gpng

grp_test:	grp_test.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

all_latency:	all_latency.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

all_lat:	all_lat.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

gpingpong:	gpingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

gpng:		gpng.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f *.o *.d core


include $(PANDA)/include/appl.mk
