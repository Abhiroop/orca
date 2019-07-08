# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
MACH		= sparc_sunos4
CC		= gcc
AR		= ar
ARFLAGS		= r
RANLIB		= ranlib

# architecture-specific defines
DEFINES		= $(SPEC_DEFINES) -DNEWRPC
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O2) \
		  $(EXTRAOPTIM:Y=-finline-functions -fomit-frame-pointer)

SYS_INCLUDES	= -I$(PANDA_PROJ)/$(VERSION)/include/sys/nmd4_sparc

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
