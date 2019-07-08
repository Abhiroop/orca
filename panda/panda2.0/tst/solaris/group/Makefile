PANDA		:= /usr/proj/panda/panda2.0
VERSION		:= latest_greatest
# FLAVOUR		:= optimized
ARCH		:= solaris/$(FLAVOUR)

SRC_DIR		:= $(PANDA)/tst/src/group
# SRC_DIR		:= .

CPPFLAGS	+= -DNDEBUG
CPPFLAGS	+= -DSTATISTICS

CFLAGS		+= -g -ansi -pedantic
CFLAGS		+= -Wall -Wmissing-prototypes -Wstrict-prototypes
CFLAGS		+= -O2 -finline-functions

default:	grp_test

grp_test:	grp_test.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

all_lat:	all_lat.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

gpng:		gpng.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

gpingpong:	gpingpong.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
