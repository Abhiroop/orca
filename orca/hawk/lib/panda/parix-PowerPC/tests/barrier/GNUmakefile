RTS		:= $(shell pwd)/../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/parix-PowerPC

ARCH		:= parix-PowerPC

SRC_DIR		:= $(RTS)/tests/barrier

CPPFLAGS	:= -DTIM # -DDUMP
CFLAGS		:= 
LIBS		:=

.PHONY: all
all:		barrier

barrier:	barrier.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

include $(RTS)/include/make/appl.mk

