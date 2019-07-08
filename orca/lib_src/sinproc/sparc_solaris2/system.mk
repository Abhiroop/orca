# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
MACH		= sparc_solaris2
CC		= gcc
AR		= ar
ARFLAGS		= r
RANLIB		= :

# architecture-specific defines
DEFINES		= $(SPEC_DEFINES) -DSOLARIS2 -D_REENTRANT
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O2) \
		  $(EXTRAOPTIM:Y=-finline-functions -fomit-frame-pointer)

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
