RTS		:= $(shell pwd)/../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/amoeba

ARCH		:= amoeba

SRC_DIR		:= $(RTS)/tests/barrier

CPPFLAGS	:= -DTIM
CFLAGS		:= -g -ansi -Wall -Wstrict-prototypes -Wmissing-prototypes
LIBS		:=

.PHONY: all
all:		barrier

barrier:	barrier.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		del -f /home/barrier
		ainstall barrier /home/barrier

include $(RTS)/include/make/appl.mk

