PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= linux

SRC_DIR		:= $(PANDA)/tst/src/system/sliding

# Additional local flags
CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+=

SRC		= slid.c \
		  tst.c

OBJ		= $(SRC:%.c=%.o)

slid:		$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:		
		rm -f Makefile.bak panda_adm *.o *.d saw core

include $(PANDA)/include/appl.mk
