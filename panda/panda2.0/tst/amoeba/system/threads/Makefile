PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba/optimized

SRC_DIR		:= $(PANDA)/tst/src/system/threads

CPPFLAGS	+= \
		-D_REENTRANT -DPACKET_SIZE=8192

CFLAGS		+= \
		-g -O2 -finline-functions -ansi -pedantic \
		-Wall -Wmissing-prototypes -Wstrict-prototypes

LIBS		+= -lm

default:	threads

threads:	threads.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
