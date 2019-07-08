PANDA           := /usr/proj/panda/panda2.0
ARCH            := linux

SRC_DIR		:= $(PANDA)/tst/src/bg

# Additional local flags
CPPFLAGS        += -UCHECK -D_MIT_POSIX_THREADS -D_POSIX_THREAD_SAFE_FUNCTIONS
CFLAGS          += -g -ansi -Wall -pedantic -Wstrict-prototypes \
		   -Wmissing-prototypes
LDFLAGS         +=

.PHONY: all
all:		test1 test2 #test3

test1:		test1.o
		$(LD) $(LDFLAGS) -o test1 $^ $(LIBS)

test2:		test2.o
		$(LD) $(LDFLAGS) -o test2 $^

test3:		test3.o $(LIBS)
		$(LD) $(LDFLAGS) -o test3 $^

.PHONY: clean
clean:
		rm -f *.o test? core gmon.out

include $(PANDA)/include/appl.mk
