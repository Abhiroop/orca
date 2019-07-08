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
CPPFLAGS        += $(AMCDEFINES)
CPPFLAGS        += -I$(AMINC)
CPPFLAGS        += -I$(AMINC)/server
CPPFLAGS        += -I$(AMINC)/posix
CPPFLAGS        += -I$(AMINC)/toolset/gnu-2

CPPFLAGS	+= -I$(RTS)/include
CPPFLAGS	+= -I$(RTS)/include/synchronization
CPPFLAGS	+= -I$(RTS)/include/communication
CPPFLAGS	+= -I$(RTS)/include/util
CPPFLAGS	+= -I$(RTS)/include/po

CPPFLAGS	+= -I$(PANDA)/lib/amoeba
CPPFLAGS	+= -Dmalloc=pan_malloc -Dfree=pan_free \
		   -Drealloc=pan_realloc -Dcalloc=pan_calloc

CPPFLAGS	+= #-DTIM -DUSE_BG

CFLAGS          := -g3 -ansi -Wall -Wstrict-prototypes -Wmissing-prototypes \
		  -mv8
 
CC              := $(TOOLSET)/do_gcc $(AMCFLAGS) 

DEP		:= $(CC) -MM

# ar
ARFLAGS         := crs
AR              := $(TOOLSET)/do_ar $(AMCFLAGS)


OUTPUT_OPTION	=
export OUTPUT_OPTION

# Include common master makefile
include		$(RTS)/include/make/master.mk


# Copy source tree to amoeba
src_dirs	:= $(addprefix $(RTS)/, $(MODULES))
find_sources 	= $(wildcard $(dir)/*.[ch])
c_files    	= $(foreach dir, $(src_dirs), $(find_sources))

inc_dirs	:= $(addprefix $(RTS)/include/, $(MODULES)) $(RTS)/include
find_include	= $(wildcard $(dir)/*.h)
h_files		= $(foreach dir, $(inc_dirs), $(find_include))

tree:		$(c_files) $(h_files)
		@for from in $?; do \
			to=/home/saniya/`echo $$from | sed 's|$(RTS)/||'`; \
			echo Updating: $$to; \
			del -d $$to >/dev/null 2>&1; \
			tob $$from $$to; \
		done
		@touch tree


