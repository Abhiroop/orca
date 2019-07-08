PANDA		:=
VERSION		:= 
ARCH		:= parix2.1-PowerPC

SRC_DIR		:= $(PANDA)/tst/src/rpc

# Additional local flags
CPPFLAGS	+= -g #-DDEBUG
CFLAGS		+= -g
LIBS		+=

all:		test1 test2 test3 test4 test5 test_n tst_n
# all:		test_n

test_n:		test_n.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

tst_n:		tst_n.o -lpanda
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


clean:	
		rm -f *.o test_n test? Makefile.bak core gmon.out

include $(PANDA)/include/appl.mk
