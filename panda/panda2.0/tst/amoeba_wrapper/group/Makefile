PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_wrapper/optimized

SRC_DIR		:= $(PANDA)/tst/src/group

CPPFLAGS	+= \
		-I/usr/proj/amwork/src/h \
		-DNDEBUG

CFLAGS		+= \
		-g -ansi -pedantic -O2 -finline-functions -mv8 \
		-Wall -Wmissing-prototypes -Wstrict-prototypes

default:	grp_test

grp_test:	grp_test.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
