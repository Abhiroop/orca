# Makefile for the Linux version of Panda 2.0

# Panda root directory
PANDA		:= /net/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/bare_bones util trace trace/tools

# Architecture specific options
CC		:= gcc
CPPFLAGS	+= -I$(PANDA)/include -D_MIT_POSIX_THREADS \
		   -D_POSIX_THREAD_SAFE_FUNCTIONS
CFLAGS		+= -O2 -finline-functions \
		   -ansi -Wall -pedantic -Wstrict-prototypes \
		   -Wmissing-prototypes # -fPIC
LD		:= gcc
LDFLAGS		+=

ARCH            := linux/optimized/bare_bones
VERSION         :=
export ARCH VERSION

# Module specific flags
GROUP_CPPFLAGS	= -DSTATISTICS -DMSG_COUNT
SYSTEM_CPPFLAGS := -DMULTICAST -DSTATISTICS  # -DBROADCAST_SKIP_SRC

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
