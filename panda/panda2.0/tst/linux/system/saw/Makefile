PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= linux

SRC_DIR		:= $(PANDA)/tst/src/system/saw

SRC		:= saw.c \
		   tst.c

CPPFLAGS	+=
CFLAGS		+= -g
LDFLAGS		+= 

OBJ		:= $(SRC:%.c=%.o)

main:		saw

include $(PANDA)/include/appl.mk

saw:		$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
		-rm -f saw* tst* core

