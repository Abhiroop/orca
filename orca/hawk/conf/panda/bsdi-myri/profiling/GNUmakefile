# RTS root directory
RTS		:= $(shell pwd)/../../../../rts

# All modules that should be included in the library
MODULES		:= \
		communication/panda \
		synchronization/panda \
		collection.reliable \
		util \
		po \

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/home/orca_panda/panda/panda3.0
endif

# C compiler
CPPFLAGS        := -I$(PANDA)/include

CPPFLAGS	+= -I$(RTS)/include
CPPFLAGS	+= -I$(RTS)/include/synchronization
CPPFLAGS	+= -I$(RTS)/include/communication
CPPFLAGS	+= -I$(RTS)/include/collection
CPPFLAGS	+= -I$(RTS)/include/util
CPPFLAGS	+= -I$(RTS)/include/po

# hacks to avoid naming conflicts
CPPFLAGS	+= -D_POSIX_C_SOURCE -D_SYS_MACHTYPES_H
CPPFLAGS	+= -D_TIMER_T

CPPFLAGS	+= -DTIM #-DUSE_BG
CPPFLAGS	+= -DPRECONDITION_ON
CPPFLAGS        += -Dmalloc=pan_malloc -Dfree=pan_free \
		   -Drealloc=pan_realloc -Dcalloc=pan_calloc

CPPFLAGS	+= -I$(PANDA)/lib/intel_BSD.myri/ot
CPPFLAGS	+= -DRELIABLE_COMM

CFLAGS          := -g -ansi -O2 -pg
 
CC              := gcc

DEP		:= $(CC) -MM

RANLIB		:= ranlib

# Include common master makefile
include		$(RTS)/include/make/master.mk

