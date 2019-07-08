# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.4 1995/07/31 09:13:48 ceriel Exp $

include		../make_paths
include		../make_params

# Fixed:

MACH		= $(PROC)_$(OS)
SRC_DIR		= $(ORCA_HOME)/src/comp
DEFINES		=
LIBDIR		= ../libs

VERSION		= lib

default:	all

ALWAYS::

all:		Makefile.main
		$(MAKE) $(MFLAGS) -f Makefile.main \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			SRC_DIR="$(SRC_DIR)" \
			DEFINES="$(DEFINES)" \
			EXT_FLAGS="$(EXT_FLAGS)" \
			MACH="$(MACH)" \
			main

install:	all
		-mkdir -p $(ORCA_TARGET)/$(VERSION)
		/bin/cp main $(ORCA_TARGET)/$(VERSION)/oc_c.$(MACH)

cmp:		all
		-cmp main $(ORCA_TARGET)/$(VERSION)/oc_c.$(MACH)

lpr:
		$(MAKE) $(MFLAGS) pr | lpr

pr:
		@$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make pr

lint:		Makefile.main
		$(MAKE) $(MFLAGS) -f Makefile.main \
			LINT="$(LINT)" \
			LIBDIR="$(LIBDIR)" \
			SRC_DIR="$(SRC_DIR)" \
			DEFINES="$(DEFINES)" \
			LINTFLAGS="$(LINTFLAGS)" \
			lint

depend:
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			SRC_DIR="$(SRC_DIR)" \
			DEFINES="$(DEFINES)" \
			MAKEDEPEND="$(MAKEDEPEND)" \
			depend

Makefile.main:	$(SRC_DIR)/proto.make ALWAYS
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			EXT_FLAGS="$(EXT_FLAGS)" \
			SRC_DIR="$(SRC_DIR)" \
			DEFINES="$(DEFINES)" \
			MAKEDEPEND="$(MAKEDEPEND)" \
			Makefile.main

clean:
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make clean
		/bin/rm -f Makefile.main*
