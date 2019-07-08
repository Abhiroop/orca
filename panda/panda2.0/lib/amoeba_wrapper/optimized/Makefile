# Makefile for the Amoeba version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= \
		   system/amoeba_wrapper \
		   amoeba_wrapper/group \
		   amoeba_wrapper/rpc \
		   amoeba_wrapper/mp \
		   amoeba_wrapper/util

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
CPPFLAGS        += -I$(AMINC)
CPPFLAGS        += -I$(AMINC)/server
CPPFLAGS        += -I$(AMINC)/posix
CPPFLAGS        += -I$(AMINC)/toolset/gnu-2
#CPPFLAGS	+= -DQPT
CPPFLAGS	+= -DNDEBUG
#CPPFLAGS	+= -DTIM
#CPPFLAGS	+= -DBROADCAST_SKIP_SRC
CPPFLAGS	+= -DTHREAD_VISIBLE
CPPFLAGS	+= -DKOEN
 
#CFLAGS          += -O2 -finline-functions -mv8 
CFLAGS		+= -g -ansi -Wall -pedantic -O2 -finline-functions -mv8 \
		   -Wstrict-prototypes -Wmissing-prototypes
 
CC              := $(TOOLSET)/do_gcc $(AMCFLAGS) $(AMCDEFINES)

# ar
ARFLAGS         := crs
AR              := $(TOOLSET)/do_ar $(AMCFLAGS)


OUTPUT_OPTION	:=
export OUTPUT_OPTION

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
