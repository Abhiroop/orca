PANDA           := /usr/proj/panda/panda2.0/latest_greatest
ARCH            := solaris_tim

SRC_DIR		:= $(PANDA)/tst/src/bg2
 
# Additional local flags
CPPFLAGS        += -UCHECK
CFLAGS          += -g -ansi -Wall -pedantic -Wstrict-prototypes \
		   -Wmissing-prototypes
LDFLAGS         +=

.PHONY: all
all:		test1 test2 test3

test1:		test1.o -lpanda
		$(LD) $(LDFLAGS) -o test1 $^ $(LIBS) 

test2:		test2.o -lpanda
		$(LD) $(LDFLAGS) -o test2 $^ $(LIBS)

test3:		test3.o -lpanda
		$(LD) $(LDFLAGS) -o test3 $^ $(LIBS)

.PHONY: clean
clean:
		rm -f *.o test? core gmon.out

include $(PANDA)/include/appl.mk
