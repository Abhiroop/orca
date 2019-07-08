# Include this Makefile from the architecture specific makefile, defining:
#
# PANDA
# ARCH

SRC_DIR		:= $(PANDA)/tst/src/bg2
 
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
