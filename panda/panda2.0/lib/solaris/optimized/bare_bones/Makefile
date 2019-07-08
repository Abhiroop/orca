# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Makefile for the Solaris version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/bare_bones util trace trace/tools

# Architecture specific options
CC		:= gcc
CPPFLAGS	:= -I$(PANDA)/include
CFLAGS		:= \
		-g -O2 -finline-functions \
		-ansi -Wall -Wstrict-prototypes -Wmissing-prototypes

ARCH		:= solaris/optimized/bare_bones
export ARCH LIBS LDFLAGS

# Module specific flags
GROUP_CPPFLAGS	= -DSTATISTICS -DMSG_COUNT
SYSTEM_CPPFLAGS	= -DSTATISTICS

# Export module specific flags
export GROUP_CPPFLAGS
export SYSTEM_CPPFLAGS

# Include common master makefile
include		$(PANDA)/include/master.mk

install:
		$(MAKE) -C $(PANDA_LIB)/objects/trace/tools \
			-f $(PANDA)/src/trace/tools/Makefile \
			$@

# Local Variables:
# compile-command: "gmake"
# End:
