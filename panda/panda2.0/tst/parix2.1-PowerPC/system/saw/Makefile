PANDA		:=
VERSION		:= 
ARCH		:= parix2.1-PowerPC

SRC_DIR		:= $(PANDA)/tst/src/system/saw

SRC		:= saw.c \
		   tst.c

CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+= 

OBJ		:= $(SRC:%.c=%.o)

saw:		$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
		-rm -f saw* tst* core*

include $(PANDA)/include/appl.mk

