# $Id: template.mk,v 1.2 1995/06/28 08:07:32 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= system
PWD		= $(RTS_SRC)/system/bcast-rpc

# the source files
SRC		= \
		alloc.c \
		args.c \
		cp.c \
		fork.c \
		main.c \
		message.c \
		obj_init.c \
		operation.c \
		print.c \
		scheduler.c \
		time.c \
		trace_server.c \
		unix.c \
		wakeup.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
