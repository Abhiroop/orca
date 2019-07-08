PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba/optimized

SRC_DIR		:= $(PANDA)/tst/src/group

CPPFLAGS	+= 
CFLAGS		+= \
		-O2 -finline-functions -g -ansi -pedantic \
		-Wall -Wmissing-prototypes -Wstrict-prototypes

default:	grp_test

grp_test:	grp_test.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
