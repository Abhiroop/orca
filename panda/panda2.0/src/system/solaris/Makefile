# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Module makefile for Solaris system layer

# Add module specific flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -D_REENTRANT -DPACKET_SIZE=8192
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -ansi flag, so that we can de signal handling. 
CFLAGS          := $(filter-out -ansi, $(CFLAGS))
 
SRC		:= \
		pan_sync.c \
		pan_threads.c \
		pan_error.c \
		pan_glocal.c \
		pan_time.c \
		pan_system.c \
		pan_pset.c \
		pan_nsap.c \
		pan_message.c \
		pan_comm.c \
		pan_buffer.c \
		pan_fragment.c \
		pan_sys_pool.c \
		pan_malloc.c

MODULE		:= $(PANDA)/src/system/solaris
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
