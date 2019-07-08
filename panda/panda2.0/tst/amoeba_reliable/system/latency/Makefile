PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable/optimized

SRC_DIR		:= $(PANDA)/tst/amoeba_reliable/system/latency

CPPFLAGS	+= \
		-DSTATISTICS -DMSG_COUNT \
		-I$(PANDA)/src/system/amoeba \
		-I/usr/proj/amwork/src/h \
		-DPACKET_SIZE=7304 \
		-DNDEBUG

CFLAGS		+=

default:	lat

lat:		tst.o lat.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
