# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(UTIL_CPPFLAGS)
CFLAGS		+= $(UTIL_CFLAGS)

SRC		:= \
		trc_filter.c \
		trc2ascii.c \
		trc2picl.c \
		trc2upshot.c \
		trc2xab.c \
		trc_bind_op.c \
		trc_event_tp.c \
		trc_io.c \
		trc_lib.c \
		trc_trace.c

MODULE		:= $(PANDA)/src/trace

include $(PANDA)/include/module.mk
