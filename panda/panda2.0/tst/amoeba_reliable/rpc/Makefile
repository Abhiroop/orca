PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable

SRC_DIR		:= $(PANDA)/tst/src/rpc

# Additional local flags
CPPFLAGS	+=
CFLAGS		+= -g
LIBS		+=

.PHONY: all
all:		test_n

test_n:		test_n.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test1:		test1.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test2:		test2.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test3:		test3.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test4:		test4.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test5:		test5.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test6:		test6.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

test7:		test7.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

.PHONY: clean
clean:	
		rm -f *.o test? Makefile.bak core gmon.out


include $(PANDA)/include/appl.mk
