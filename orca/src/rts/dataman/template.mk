# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: template.mk,v 1.5 1996/10/09 14:04:52 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= dataman
PWD		= $(RTS_SRC)/$(MODULE)

# the source files
SRC		= \
		array.c \
		graph.c \
		misc.c \
		tp_descr.c \
		bag.c \
		marshall.c \
		pobject.c \
		set.c 

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# DO NOT DELETE THIS LINE -- make depend depends on it.
