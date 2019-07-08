# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Makefile for the Solaris version of Panda 2.0

# Panda root directory
PANDA		:= /usr/proj/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/solaris mp rpc bg util group

# Architecture specific options
CC		:= gcc
CPPFLAGS	:= -I$(PANDA)/include
CFLAGS		:= -O2 -pg \
		   -g -ansi -Wall -pedantic -Wstrict-prototypes \
		   -Wmissing-prototypes

# Module specific flags
# GROUP_CPPFLAGS	:= -DSTATISTICS -DMSG_COUNT
SYSTEM_CPPFLAGS := -DBROADCAST_SKIP_SRC -DNOVERBOSE
MP_CPPFLAGS     := -DNOVERBOSE

# Export module specific flags
export GROUP_CPPFLAGS
export SYSTEM_CPPFLAGS MP_CPPFLAGS

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
