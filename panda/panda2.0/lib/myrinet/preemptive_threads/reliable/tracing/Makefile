# Makefile for the Amoeba version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/myrinet/preemptive_threads rpc/rel_msg util group/ord_msg trace


# Want to make executables
ARCH		:= myrinet/optimized
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
CPPFLAGS        += -I$(AMINC)/machdep/arch/sparc
CPPFLAGS        += -I$(AMCONF)/sparc.gnu-2/kernel/sun4m/pool
CPPFLAGS        += -I/usr/proj/orca/Networks/FM/myri-fm-1.1-am/include
#CPPFLAGS        += -DMYRINET -DPOLL_ON_WAIT
CPPFLAGS	+= -DNOVERBOSE
CPPFLAGS	+= -DNDEBUG
CPPFLAGS        += -DTRACING

CFLAGS          = -mv8 \
		  -O2 -finline-functions \
		  -g -ansi -Wall -Wstrict-prototypes \
                  -Wmissing-prototypes
 
AS              = $(TOOLSET)/do_gcc -c $(AMCFLAGS) $(AMCDEFINES)
CC              = $(TOOLSET)/do_gcc $(AMCFLAGS) $(AMCDEFINES)

# ar
ARFLAGS         = crs
AR              = $(TOOLSET)/do_ar $(AMCFLAGS)

# SYSTEM_CPPFLAGS := -DBROADCAST_SKIP_SRC -DNOVERBOSE
SYSTEM_CPPFLAGS := -DNOVERBOSE
SYSTEM_CPPFLAGS += -UDO_TIMINGS
SYSTEM_CPPFLAGS += -USTATISTICS
SYSTEM_CPPFLAGS += -UPANDA_RCVE_POOL
SYSTEM_CPPFLAGS += -DRELIABLE_NETWORK
SYSTEM_CPPFLAGS += -DNO_IDLE_THREAD
# SYSTEM_CPPFLAGS	+= -DNO_SYNC_MACROS
# SYSTEM_CPPFLAGS	+= -DLOCK_WRITE_OWNER
# SYSTEM_CPPFLAGS       += -DTHREAD_STATISTICS

MP_CPPFLAGS     := -DNOVERBOSE
MP_CPPFLAGS     += -DRELIABLE_UNICAST

GROUP_CPPFLAGS  += -USTATISTICS

UTIL_CPPFLAGS  := -DOS_FRAGMENTS

export SYSTEM_CPPFLAGS MP_CPPFLAGS GROUP_CPPFLAGS UTIL_CPPFLAGS


OUTPUT_OPTION	:=
export OUTPUT_OPTION

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
