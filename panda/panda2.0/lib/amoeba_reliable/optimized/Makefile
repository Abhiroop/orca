# Makefile for the Amoeba version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/amoeba_reliable group/ordered util mp rpc



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
CPPFLAGS	+= -DNDEBUG -DNOVERBOSE -DRELIABLE_UNICAST -DORDERED_MULTICAST
 
CFLAGS          = -g -ansi -Wall -Wstrict-prototypes \
                  -Wmissing-prototypes -mv8 -O2 -finline-functions
 
CC              = $(TOOLSET)/do_gcc $(AMCFLAGS)

# ar
ARFLAGS         = crs
AR              = $(TOOLSET)/do_ar $(AMCFLAGS)

# Module-specific flags
GROUP_CPPFLAGS =
SYSTEM_CPPFLAGS	= # -DSTATISTICS -DBROADCAST_SKIP_SRC

# Export module-specific flags
export GROUP_CPPFLAGS
export SYSTEM_CPPFLAGS

OUTPUT_OPTION	=
export OUTPUT_OPTION

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
