PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable

# SRC_DIR		:= $(PANDA)/tst/src/system/comm
SRC_DIR		:= $(PANDA)/tst/amoeba_reliable/system/comm

CPPFLAGS	+= \
		-DSTATISTICS -DMSG_COUNT \
		-I$(PANDA)/src/system/amoeba \
		-I/usr/proj/amwork/src/h \
		-DPACKET_SIZE=7304 -DNO_MCAST_FROM_UPCALL

CFLAGS		+= \
		-g -ansi -pedantic \
		-Wall -Wmissing-prototypes -Wstrict-prototypes -mv8

# default:	mcast unicast msgbuild throughput
default:	mcast_upc

mcast_upc:	mcast_upc.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

mcast:		mcast.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

unicast:	unicast.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

msgbuild:	msgbuild.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

throughput:	throughput.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
