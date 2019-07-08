# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: template.mk,v 1.4 1995/07/31 09:06:56 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= system
PWD		= $(RTS_SRC)/system/panda2.0

# the source files
SRC		= \
		manager.c \
		account.c \
		Finish.c \
		invocation.c \
		continuation.c \
		Time.c \
		obj_tab.c \
		main.c \
		msg_marshall.c \
		process.c \
		unix.c \
		fork_exit.c \
		proxy.c \
		fragment.c \
		thrpool.c \
		rts_comm.c \
		rts_init.c \
		rts_internals.c \
		rts_measure.c \
		rts_object.c \
		rts_trace.c \
		trc.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
