# $Id: template.mk,v 1.2 1995/06/28 08:16:26 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= system
PWD		= $(RTS_SRC)/system/sinproc

# the source files
SRC		= \
		DoFork.c \
		Finish.c \
		alloc.c \
		print.c \
		DoOperation.c \
		Time.c \
		main.c \
		unix.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
