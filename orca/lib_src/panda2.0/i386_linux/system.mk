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

# SYS_TARGETS     = sharedlib

# architecture-specific defines
DEFINES		= $(SPEC_DEFINES) -D_MIT_POSIX_THREADS \
                  -D_POSIX_THREAD_SAFE_FUNCTIONS
SHAREDFLAG	=
# SHAREDFLAG	= -fPIC
EXT_FLAGS	= $(SHAREDFLAG) \
                  $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O2) \
		  $(EXTRAOPTIM:Y=-finline-functions -fomit-frame-pointer) \

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk

sharedlib: $(LIB_TARGETS)
	$(CC) -shared -Wl,-soname,liborca.so.1 -o liborca.so.1.0 \
		$(PWD)/objects/*.o
	ln -sf liborca.so.1.0 liborca.so.1
