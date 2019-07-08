PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= linux

SRC_DIR		:= $(PANDA)/tst/src/system/threads

# Additional local flags
CPPFLAGS	+=
CFLAGS		+= -g
LDFLAGS		+= -g

SRC		= threads.c

OBJ		= $(SRC:%.c=%.o)

main:		threads

include $(PANDA)/include/appl.mk

threads:	$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:          
		rm -f Makefile.bak panda_adm *.o *.d core
