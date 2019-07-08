# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.4 1997/06/16 13:08:53 ceriel Exp $

include ../make_paths
include ../make_params

BINDIR		= $(ORCA_TARGET)/bin.$(BINSUF)
LIBDIR		= $(ORCA_TARGET)/lib/LLgen
MANDIR		= $(ORCA_TARGET)/man/man1

all:
	cd src; $(MAKE) $(MFLAGS) LIBDIR=$(LIBDIR) first

clean:
	-cd src; $(MAKE) $(MFLAGS) clean

install:
	cd src; $(MAKE) $(MFLAGS) LIBDIR=$(LIBDIR) first
	-mkdir -p $(BINDIR)
	/bin/cp src/LLgen $(BINDIR)/LLgen
	-mkdir -p $(LIBDIR)
	-mkdir -p $(MANDIR)
	/bin/cp $(ORCA_HOME)/src/LLgen/lib/rec $(LIBDIR)/rec
	/bin/cp $(ORCA_HOME)/src/LLgen/lib/incl $(LIBDIR)/incl
	/bin/cp $(ORCA_HOME)/src/LLgen/lib/nc_rec $(LIBDIR)/nc_rec
	/bin/cp $(ORCA_HOME)/src/LLgen/lib/nc_incl $(LIBDIR)/nc_incl
	/bin/cp $(ORCA_HOME)/src/LLgen/LLgen.1 $(MANDIR)/LLgen.1

distr:
	cd src; $(MAKE) $(MFLAGS) distr
