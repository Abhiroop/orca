# RTS root directory
RTS		:= $(shell pwd)/../../../../rts

# All modules that should be included in the library
MODULES		:= \
		communication/panda \
		synchronization/panda \
		collection \
		util \
		po \

PANDA		:= /usr/proj/panda/panda3.0

# C compiler
CPPFLAGS        := -I$(PANDA)/include
CPPFLAGS        += -I$(PANDA)/lib/solaris

CPPFLAGS	+= -I$(RTS)/include
CPPFLAGS	+= -I$(RTS)/include/synchronization
CPPFLAGS	+= -I$(RTS)/include/communication
CPPFLAGS	+= -I$(RTS)/include/util
CPPFLAGS	+= -I$(RTS)/include/po

# hacks to avoid naming conflicts
CPPFLAGS	+= -D_POSIX_C_SOURCE -D_SYS_MACHTYPES_H
CPPFLAGS	+= -D_TIMER_T

CPPFLAGS	+= -DTIM -DUSE_BG
CPPFLAGS        += -Dmalloc=pan_malloc -Dfree=pan_free \
		   -Drealloc=pan_realloc -Dcalloc=pan_calloc

CFLAGS          := -g -ansi -Wall -Wstrict-prototypes -Wmissing-prototypes
 
CC              := gcc

DEP		:= $(CC) -MM

# Include common master makefile
include		$(RTS)/include/make/master.mk

