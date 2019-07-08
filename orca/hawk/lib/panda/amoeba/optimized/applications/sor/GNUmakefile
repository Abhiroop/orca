RTS		:= $(shell pwd)/../../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/amoeba/optimized

ARCH		:= amoeba/optimized

SRC_DIR		:= $(RTS)/applications/sor/C

CPPFLAGS	:=
CFLAGS		:= -g -O2 -finline-functions
LIBS		:=

.PHONY: all
all:		sor

sor:		sor.o grid.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		-del -f /home/sor
		-ainstall sor /home/sor

include $(RTS)/include/make/appl.mk

