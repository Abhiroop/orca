# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(RPC_CPPFLAGS)
CFLAGS		+= $(RPC_CFLAGS)

SRC		:= \
		pan_rpc.c \
		pan_upcall.c

MODULE		:= $(PANDA)/src/rpc

include $(PANDA)/include/module.mk
