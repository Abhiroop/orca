PANDA		:= /usr/proj/panda/panda2.0/latest_greatest
ARCH		:= amoeba_tim

SRC_DIR		:= $(PANDA)/tst/src/system/saw

SRC		:= saw.c \
		   tst.c

CPPFLAGS	+=
CFLAGS		+= -g -ansi -Wall -pedantic
LDFLAGS		+= 

OBJ		:= $(SRC:%.c=%.o)

saw:		$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
		-rm -f saw* tst* core

include $(PANDA)/include/appl.mk

