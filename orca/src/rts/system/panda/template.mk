# $Id: template.mk,v 1.2 1995/06/28 08:08:25 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= system
PWD		= $(RTS_SRC)/system/panda

# the source files
SRC		= \
		manager.c \
		account.c \
		Finish.c \
		invocation.c \
		rts_init.c \
		continuation.c \
		Time.c \
		obj_tab.c \
		rts_internals.c \
		main.c \
		process.c \
		rts_object.c \
		unix.c \
		fork_exit.c \
		proxy.c \
		rts_trace.c \
		fragment.c \
		msg_marshall.c \
		rts_util.c \
		thrpool.c \
		rts_comm.c \
		trc.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
