RTS		:= $(shell pwd)/../../../../..

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

RTS_CONF	:= panda/amoeba

VERSION		:= latest_greatest
ARCH		:= amoeba_tim

SRC_DIR		:= $(RTS)/applications/jacobi/C

CPPFLAGS	+=
CFLAGS		+= -g
LIBS		+=

.PHONY: all
all:		jacobi

jacobi:		jacobi.o vector.o -lrts -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
		-del -f /home/jacobi
		-ainstall jacobi /home/jacobi

include $(RTS)/include/make/appl.mk

