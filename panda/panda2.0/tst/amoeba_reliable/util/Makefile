PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable

SRC_DIR		:= $(PANDA)/tst/src/util

CPPFLAGS	+= \
		-DSTATISTICS -DMSG_COUNT \
		-I$(PANDA)/src/util \
		-I$(PANDA)/src/system/solaris \
		-D_REENTRANT -DPACKET_SIZE=8192

CFLAGS		+= \
		-g -ansi -pedantic \
		-Wall -Wmissing-prototypes -Wstrict-prototypes

LIBS		+= -lm

default:	clck_sync

clck_sync:	clck_sync.o pan_clck_syn.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
