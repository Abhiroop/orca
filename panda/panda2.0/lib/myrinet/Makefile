# Makefile for the Amoeba version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/myrinet rpc/rel_msg group/ord_msg util


# Architecture for baking binaries
ARCH		:= amoeba
export ARCH

# Amoeba-specific stuff
AMWORK          := /usr/proj/amwork
AMCONF          := $(AMWORK)/conf/amoeba
 
TOOLSET         := $(AMCONF)/sparc.gnu-2/toolset
AMOEBA_BIN      := $(AMWORK)/bin.sun4/gnu
AMINC           := $(AMWORK)/src/h
 
AMCFLAGS        := -mam_sparc -G $(AMOEBA_BIN)
AMCDEFINES      := -Dsparc -DAMOEBA -nodeps
 
# C compiler
CPPFLAGS        += -I$(PANDA)/include
CPPFLAGS        += $(AMCDEFINES)
CPPFLAGS        += -I$(AMINC)
CPPFLAGS        += -I$(AMINC)/server
CPPFLAGS        += -I$(AMINC)/posix
CPPFLAGS        += -I$(AMINC)/toolset/gnu-2
CPPFLAGS        += -I/usr/proj/orca/Networks/FM/myri-fm-1.1-am/include
CPPFLAGS        += -DMYRINET -DPOLL_ON_WAIT
 
CFLAGS          = -g -ansi -Wall -pedantic -Wstrict-prototypes \
                  -Wmissing-prototypes -mv8
 
CC              = $(TOOLSET)/do_gcc $(AMCFLAGS) $(AMCDEFINES)

# ar
ARFLAGS         = crs
AR              = $(TOOLSET)/do_ar $(AMCFLAGS)


OUTPUT_OPTION	=
export OUTPUT_OPTION

SYSTEM_CPPFLAGS	:= # -DBROADCAST_SKIP_SRC
SYSTEM_CPPFLAGS	+= -DNOVERBOSE
SYSTEM_CPPFLAGS	+= -DSTATISTICS
SYSTEM_CPPFLAGS	+= -DDO_TIMINGS

MP_CPPFLAGS	:= -DNOVERBOSE -DRELIABLE_UNICAST

GROUP_CPPFLAGS	+= -DSTATISTICS

UTIL_CPPFLAGS	+= -DOS_FRAGMENTS

export SYSTEM_CPPFLAGS MP_CPPFLAGS GROUP_CPPFLAGS UTIL_CPPFLAGS


# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
