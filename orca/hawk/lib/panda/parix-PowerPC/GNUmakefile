# RTS root directory
RTS		:= $(shell pwd)/../../../rts

# All modules that should be included in the library
MODULES		:= \
		communication/panda \
		synchronization/panda \
		collection \
		util \
		po \

ifdef PANDA_PROJ
PANDA		:= $(PANDA_PROJ)
else
PANDA		:= /usr/proj/panda/panda3.0
endif

# Parix PowerXplorer specific stuff
 
# C compiler
CPPFLAGS        := -DPARIX -DPARIX_PowerPC
CPPFLAGS        += -DPANDA_SLEEP
CPPFLAGS        += -I$(PANDA)/include
CPPFLAGS        += -DTIM

CPPFLAGS        += -I$(RTS)/include
CPPFLAGS        += -I$(RTS)/include/synchronization
CPPFLAGS        += -I$(RTS)/include/communication
CPPFLAGS        += -I$(RTS)/include/util
CPPFLAGS        += -I$(RTS)/include/po

CFLAGS          := -g 
 
CC              := px ancc -w

DEP		:= gcc -MM
 
# ar
RANLIB          := px ar s
AR              := px ar
ARFLAGS         := rv
 

# Include common master makefile
include		$(RTS)/include/make/master.mk

