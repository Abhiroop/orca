# Makefile for the Amoeba version of Panda 2.0

PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/cmaml mp rpc util group/ordered



# C compiler
CPPFLAGS        := -I.
CPPFLAGS        += -I$(PANDA)/include
CPPFLAGS	+= -DTHREAD_VISIBLE
 
CFLAGS          := -O2 -finline-functions -mv8 
CFLAGS		+= -g -ansi -Wall -Wmissing-prototypes
 
AS              := gcc -c
CC              := gcc

# ar
ARFLAGS         := r
AR              := ar
RANLIB		:= ranlib

# Module-specific flags
GROUP_CPPFLAGS  = # -DNO_MCAST_FROM_UPCALL
MP_CPPFLAGS     = -DRELIABLE_UNICAST
SYSTEM_CPPFLAGS = -DCMOST -DBROADCAST_SKIP_SRC # -DSTATISTICS 
 
# Export module-specific flags
export GROUP_CPPFLAGS
export MP_CPPFLAGS
export SYSTEM_CPPFLAGS
 
OUTPUT_OPTION	:=
export OUTPUT_OPTION

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:


