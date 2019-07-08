# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: template.mk,v 1.4 1998/06/11 12:00:59 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= system
PWD		= $(RTS_SRC)/system/unixproc

# the source files
SRC		= \
		DoFork.c \
		Finish.c \
		alloc.c \
		print.c \
		DoOperation.c \
		Time.c \
		main.c \
		unix.c \
		test_marshall.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
