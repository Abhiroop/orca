# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(MP_CPPFLAGS)
CFLAGS		+= $(MP_CFLAGS)

SRC		:= \
		pan_mp_conn.c \
		pan_mp_state.c \
		pan_mp_init.c \
		pan_mp_error.c \
		pan_mp_ticks.c \
		pan_mp_queue.c

MODULE		:= $(PANDA)/src/mp

include $(PANDA)/include/module.mk
