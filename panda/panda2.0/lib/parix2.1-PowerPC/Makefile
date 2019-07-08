# Makefile for the Parix PowerXplorer version of Panda 2.1

# Panda root directory
PANDA		:=

# All modules that should be included in the library
MODULES		:= mp rpc group/ordered util
SYSTEM		:= parix2.1

# Architecture for baking binaries
ARCH		:= parix2.1-PowerPC
export ARCH

# Parix PowerXplorer specific stuff

# C compiler
CPPFLAGS        := -DPARIX -DPARIX_PowerPC
CPPFLAGS        += -DSTATISTICS -DMSG_COUNT
CPPFLAGS        += -DPACKET_SIZE=8192
CPPFLAGS        += -DNO_MCAST_FROM_UPCALL
CPPFLAGS        += -DRELIABLE_UNICAST
CPPFLAGS	+= -I$(PANDA)/include
CPPFLAGS	+= -DTIM

CFLAGS          = -g 
 
CC              = px ancc

# ar
RANLIB		= px ar s
AR              = px ar
ARFLAGS         = rv


OUTPUT_OPTION	=
export OUTPUT_OPTION

SYSTEM_CPPFLAGS	:= -DBROADCAST_SKIP_SRC -DNOVERBOSE -DSTATISTICS
MP_CPPFLAGS	:= -DNOVERBOSE
GROUP_CPPFLAGS  := -DNOVERBOSE -DSTATISTICS -DUSE_MP -DSEQUENCER=0
export SYSTEM_CPPFLAGS MP_CPPFLAGS GROUP_CPPFLAGS

# Include common master makefile
include		$(PANDA)/include/master2.1.mk
