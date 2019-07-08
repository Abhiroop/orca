# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: template.mk,v 1.7 1997/04/07 10:20:01 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= misc
PWD		= $(RTS_SRC)/$(MODULE)

# the source files
SRC		= \
		abs.c \
		range.c \
		ptrs.c \
		alias.c \
		divide.c \
		traps.c \
		info.c \
		c_intf.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
