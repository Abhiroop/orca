# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.9 1995/07/31 09:13:41 ceriel Exp $

include ../make_paths
include ../make_params

# Fixed:

MACH		= $(PROC)_$(OS)
BIN_DIR		= $(ORCA_TARGET)/bin.$(BINSUF)
SRC_DIR		= $(ORCA_HOME)/src/driver
MAN_DIR		= $(ORCA_TARGET)/man
OC_HOME		= $(ORCA_TARGET)

OC_LIBS		= $$$$OC_HOME/$$$$OC_LIBNAM/$$$$OC_RTSNAM/$$$$OC_MACH/$$$$OC_SPECIAL/lib$$$$OC_RTSNAM.a
OC_COMP		= $$$$OC_HOME/$$$$OC_LIBNAM/oc_c.$(MACH)
OC_LIBNAM	= lib
DEFINES		=
LIBDIR		= ../libs

all:		Makefile.main
		$(MAKE) $(MFLAGS) -f Makefile.main \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			DEFINES="$(DEFINES)" \
			EXT_FLAGS="$(EXT_FLAGS)" \
			MACH="$(MACH)" \
			ORCA_HOME="$(ORCA_HOME)" \
			OC_HOME="$(OC_HOME)" \
			SRC_DIR="$(SRC_DIR)" \
			OC_LIBS='$(OC_LIBS)' \
			OC_COMP='$(OC_COMP)' \
			OC_LIBNAM="$(OC_LIBNAM)" \
			main

install:	all
		-mkdir -p $(BIN_DIR)
		/bin/cp main $(BIN_DIR)/oc
		-ln -s $(BIN_DIR)/oc $(BIN_DIR)/oc_2c
		-ln -s $(BIN_DIR)/oc $(BIN_DIR)/oc_check
		-mkdir -p $(MAN_DIR)/man1
		/bin/cp $(SRC_DIR)/oc.1 $(MAN_DIR)/man1/oc.1

cmp:		all
		-cmp main $(BIN_DIR)/oc

lpr:
		$(MAKE) $(MFLAGS) pr | lpr

pr:
		@$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make pr

lint:		Makefile.main
		$(MAKE) $(MFLAGS) -f Makefile.main \
			LINT="$(LINT)" \
			LIBDIR="$(LIBDIR)" \
			LINTFLAGS="$(LINTFLAGS)" \
			ORCA_HOME="$(ORCA_HOME)" \
			OC_HOME="$(OC_HOME)" \
			MACH="$(MACH)" \
			SRC_DIR="$(SRC_DIR)" \
			OC_LIBS='$(OC_LIBS)' \
			OC_COMP='$(OC_COMP)' \
			OC_LIBNAM="$(OC_LIBNAM)" \
			lint

depend:
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			DEFINES="$(DEFINES)" \
			ORCA_HOME="$(ORCA_HOME)" \
			OC_HOME="$(OC_HOME)" \
			MACH="$(MACH)" \
			SRC_DIR="$(SRC_DIR)" \
			OC_LIBS='$(OC_LIBS)' \
			OC_COMP='$(OC_COMP)' \
			OC_LIBNAM="$(OC_LIBNAM)" \
			MAKEDEPEND="$(MAKEDEPEND)" \
			Makefile.main

Makefile.main:	$(SRC_DIR)/proto.make
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make \
			CC="$(CC)" \
			LIBDIR="$(LIBDIR)" \
			DEFINES="$(DEFINES)" \
			ORCA_HOME="$(ORCA_HOME)" \
			OC_HOME="$(OC_HOME)" \
			MACH="$(MACH)" \
			SRC_DIR="$(SRC_DIR)" \
			OC_LIBS='$(OC_LIBS)' \
			OC_COMP='$(OC_COMP)' \
			OC_LIBNAM="$(OC_LIBNAM)" \
			MAKEDEPEND="$(MAKEDEPEND)" \
			Makefile.main

clean:
		$(MAKE) $(MFLAGS) -f $(SRC_DIR)/proto.make clean
		/bin/rm -f Makefile.main*
