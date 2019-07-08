RTS		:= $(shell pwd)/../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/solaris2

ARCH		:= solaris

SRC_DIR		:= $(RTS)/applications/fft/c

CPPFLAGS	:=
CFLAGS		:= -g
LIBS		:=

.PHONY: all
all:		fft

fft:		fft.o vector.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		#purify $(LD) $(LDFLAGS) -o $@_pur $^ $(LIBS)

include $(RTS)/include/make/appl.mk

