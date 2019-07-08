# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
MACH		= parix-PowerPC
CC		= ppx ancc
AR		= ppx ar
ARFLAGS		= rv
RANLIB		= ppx ranlib

SYS_INCLUDES	= \
		-I$(PARIX)/include

DEFINES		= $(SPEC_DEFINES)
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O3) \
		  $(EXTRAOPTIM:Y=)

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
