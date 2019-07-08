# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(UTIL_CPPFLAGS)
CFLAGS		+= $(UTIL_CFLAGS)

SRC		:= \
		typed_msg.c \
		pan_util_init.c \
		pan_endian.c \
		pan_clck_msg.c \
		pan_clck_syn.c \
		pan_sleep.c \
		pan_stddev.c \
		pan_strdup.c \
		pan_time_fix.c \
		pan_time_fix2t.c

MODULE		:= $(PANDA)/src/util

include $(PANDA)/include/module.mk
