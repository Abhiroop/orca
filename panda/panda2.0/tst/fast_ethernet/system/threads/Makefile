PANDA_BASE	:= /usr/proj/panda/panda2.0
# PANDA		:= $(PANDA_BASE)
PANDA		:= $(PANDA_BASE)/latest_greatest
ARCH		:= fast_ethernet

FM_LIB		:= /usr/proj/orca/Networks/FastEthernet/lib/usched/libFM.a

# SRC_DIR		:= .
SRC_DIR		:= $(PANDA)/tst/src/system/threads

CPPFLAGS	+= -I$(PANDA_BASE)/src/system/myrinet

# CPPFLAGS	+= -DMYRINET
# CPPFLAGS	+= -DOS_FRAGMENTS
# CPPFLAGS	+= -DRELIABLE_UNICAST
# CPPFLAGS	+= -DORDERED_MULTICAST
# CPPFLAGS	+= -DRELIABLE_MULTICAST
CPPFLAGS	+= -UDO_TIMINGS
# CPPFLAGS	+= -DNDEBUG
CPPFLAGS	+= -DPOLL_ON_WAIT

CFLAGS		+= -g -ansi
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes
# CFLAGS		+= -O2 -finline-functions
# CFLAGS		+= -pg
# CFLAGS		+= -mflat

LIBS		+= -lm

threads:	threads.o -lpanda $(FM_LIB)
		$(LD) $(LDFLAGS) -o $@ threads.o $(LIBS)

.PHONY: clean
clean:
		-rm -f *.o *.d core


include $(PANDA)/include/appl.mk
