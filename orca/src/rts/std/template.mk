# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: template.mk,v 1.4 1996/10/09 14:05:28 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= std
PWD		= $(RTS_SRC)/$(MODULE)

# the source files
SRC		= \
		Random.c \
		distributions.c \
		barrier.c \
		IntObject.c \
		RowCollect.c \
		PollSequen.c \
		Math_C.c \
		Xd_C.c \
		Vogle.c

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
