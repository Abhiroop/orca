# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
MACH		= i386_linux
CC		= gcc
AR		= ar
ARFLAGS		= r
RANLIB		= ranlib

# architecture-specific defines
DEFINES		= $(SPEC_DEFINES) -DPANDA4
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O2) \
		  $(EXTRAOPTIM:Y=-finline-functions -fomit-frame-pointer)

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
