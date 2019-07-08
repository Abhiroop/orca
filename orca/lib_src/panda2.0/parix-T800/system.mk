# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
MACH		= parix-T800
CC		= px ancc -Tp$(ORCA_TARGET)/bin/ancpp-T800
AR		= px ar
ARFLAGS		= rv
RANLIB		= px ar ts

SYS_INCLUDES	= \
		-I$(PARIX)/include

DEFINES		= $(SPEC_DEFINES)
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-p) \
		  $(OPTIM:Y=-O) \
		  $(EXTRAOPTIM:Y=-OI)

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
