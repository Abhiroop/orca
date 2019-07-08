# Makefile for the Linux version of Panda 2.0

# Panda root directory
PANDA		:= /net/panda/panda2.0

# All modules that should be included in the library
MODULES		:= system/linux mp rpc bg util group

# Architecture specific options
CC		:= gcc
CPPFLAGS	+= -I$(PANDA)/include -D_MIT_POSIX_THREADS \
		   -D_POSIX_THREAD_SAFE_FUNCTIONS
CFLAGS		+= -O2 -finline-functions \
		   -g -ansi -Wall -pedantic -Wstrict-prototypes \
		   -Wmissing-prototypes # -fPIC
LD		:= gcc
LDFLAGS		+=

# Module specific flags
GROUP_CPPFLAGS	= -DSTATISTICS -DMSG_COUNT
SYSTEM_CPPFLAGS := -DMULTICAST -DNOVERBOSE #-DBROADCAST_SKIP_SRC
MP_CPPFLAGS     := -DNOVERBOSE

# Export module specific flags
export GROUP_CPPFLAGS
export SYSTEM_CPPFLAGS MP_CPPFLAGS

# Include common master makefile
include		$(PANDA)/include/master.mk


# Local Variables:
# compile-command: "gmake"
# End:
