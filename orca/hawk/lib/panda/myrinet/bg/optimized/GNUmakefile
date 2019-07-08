# RTS root directory
RTS		:= $(shell pwd)/../../../../../rts

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

# Amoeba-specific stuff
AMWORK          := /usr/proj/amwork
AMCONF          := $(AMWORK)/conf/amoeba
 
TOOLSET         := $(AMCONF)/sparc.gnu-2/toolset
AMOEBA_BIN      := $(AMWORK)/bin.sun4/gnu
AMINC           := $(AMWORK)/src/h
 
AMCFLAGS        := -mam_sparc -G $(AMOEBA_BIN)
AMCDEFINES      := -Dsparc -DAMOEBA -nodeps
 
# C compiler
CPPFLAGS        := -I$(PANDA)/include
CPPFLAGS	+= -I$(PANDA)/lib/myrinet
CPPFLAGS        += $(AMCDEFINES)
CPPFLAGS        += -I$(AMINC)
CPPFLAGS        += -I$(AMINC)/server
CPPFLAGS        += -I$(AMINC)/posix
CPPFLAGS        += -I$(AMINC)/toolset/gnu-2
CPPFLAGS	+= -DNDEBUG

CPPFLAGS	+= -I$(RTS)/include
CPPFLAGS	+= -I$(RTS)/include/synchronization
CPPFLAGS	+= -I$(RTS)/include/communication
CPPFLAGS	+= -I$(RTS)/include/util
CPPFLAGS	+= -I$(RTS)/include/po

CPPFLAGS	+= -DUSE_BG

CFLAGS          := -g3 -ansi -Wall -Wstrict-prototypes -Wmissing-prototypes \
		  -mv8 -O2 -finline-functions
 
CC              := $(TOOLSET)/do_gcc $(AMCFLAGS) $(AMCDEFINES)

DEP		:= $(CC) -MM

# ar
ARFLAGS         := crs
AR              := $(TOOLSET)/do_ar $(AMCFLAGS)


OUTPUT_OPTION	:=
export OUTPUT_OPTION

# Include common master makefile
include		$(RTS)/include/make/master.mk

