PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable

SRC_DIR		:= $(PANDA)/tst/src/group

CPPFLAGS	+= \
		-DSTATISTICS -DMSG_COUNT \
		-I$(PANDA)/src/system/amoeba \
		-I/usr/proj/amwork/src/h \
		-DPACKET_SIZE=7304

CFLAGS		+= \
		-g -ansi -pedantic \
		-Wall -Wmissing-prototypes -Wstrict-prototypes -mv8

default:	grp_test

grp_test:	grp_test.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
