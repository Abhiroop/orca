PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= solaris

SRC_DIR		:= $(PANDA)/tst/src/system/threads

# Additional local flags
CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+=

SRC		= threads.c

OBJ		= $(SRC:%.c=%.o)

threads:	$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:          
		rm -f Makefile.bak panda_adm *.o *.d core

include $(PANDA)/include/appl.mk
