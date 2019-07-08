RTS		:= $(shell pwd)/../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/solaris2

ARCH		:= solaris

SRC_DIR		:= $(RTS)/tests/barrier

CPPFLAGS	:= -DTIM #-DDUMP
CFLAGS		:= -g -ansi -Wall -Wstrict-prototypes -Wmissing-prototypes
LIBS		:=

.PHONY: all
all:		barrier

barrier:	barrier.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		#purify -chain_length=12 $(LD) $(LDFLAGS) -o $@_pur $^ $(LIBS)

include $(RTS)/include/make/appl.mk

