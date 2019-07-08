# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.7 1997/06/16 13:06:16 ceriel Exp $

include		../../make_paths
include		../../make_params

# Fixed:

SRC_DIR		= $(ORCA_HOME)/src/LLgen/src
LIBDIR		= $(ORCA_TARGET)/lib/LLgen
INCLUDES	= -I$(SRC_DIR) -I.
LIBDIRSTR	= \"$(LIBDIR)\"
DEFINES		= -DNDEBUG -DNON_CORRECTING
CFLAGS		= $(DEFINES) $(INCLUDES) $(EXT_FLAGS)
LINTFLGS	= $(LINTFLAGS) $(DEFINES) $(INCLUDES) -DNORCSID

OBJECTS = main.o gencode.o compute.o LLgen.o tokens.o \
	check.o reach.o global.o name.o sets.o \
	Lpars.o alloc.o machdep.o cclass.o savegram.o
CSRC = $(SRC_DIR)/main.c $(SRC_DIR)/gencode.c $(SRC_DIR)/compute.c \
	$(SRC_DIR)/check.c $(SRC_DIR)/reach.c $(SRC_DIR)/global.c \
	$(SRC_DIR)/name.c $(SRC_DIR)/sets.c $(SRC_DIR)/alloc.c \
	$(SRC_DIR)/machdep.c $(SRC_DIR)/cclass.c $(SRC_DIR)/save_gram.c
CFILES = LLgen.c tokens.c Lpars.c $(CSRC)
GFILES = $(SRC_DIR)/tokens.g $(SRC_DIR)/LLgen.g
FILES = $(SRC_DIR)/types.h $(SRC_DIR)/extern.h \
	$(SRC_DIR)/io.h $(SRC_DIR)/sets.h \
	$(GFILES) $(CSRC) $(SRC_DIR)/proto.make

all:		parser
		@$(MAKE) $(MFLAGS) LLgen "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LIBDIR=$(LIBDIR)"

parser:		$(GFILES)
		LLgen $(GFILES)
		@touch parser

first:		firstparser
		@$(MAKE) $(MFLAGS) LLgen "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LIBDIR=$(LIBDIR)"

firstparser:
		/bin/cp $(SRC_DIR)/LLgen.c.dist LLgen.c
		/bin/cp $(SRC_DIR)/tokens.c.dist tokens.c
		/bin/cp $(SRC_DIR)/Lpars.c.dist Lpars.c
		/bin/cp $(SRC_DIR)/Lpars.h.dist Lpars.h
		@touch parser

LLgen:		$(OBJECTS)
		$(LINK.c) -o LLgen $(OBJECTS)

lint: 		parser
		$(LINT) $(LINTFLGS) -DLIBDIR=$(LIBDIRSTR) $(CFILES)

clean:
		-/bin/rm -f *.o LL.temp LL.xxx LL.output LLgen LLgen.c tokens.c Lpars.[ch] parser

distr:
		-/bin/rm -f parser
		$(MAKE) $(MFLAGS) parser
		/bin/cp Lpars.c $(SRC_DIR)/Lpars.c.dist
		/bin/cp Lpars.h $(SRC_DIR)/Lpars.h.dist
		/bin/cp LLgen.c $(SRC_DIR)/LLgen.c.dist
		/bin/cp tokens.c $(SRC_DIR)/tokens.c.dist

LLgen.o:	LLgen.c
		$(COMPILE.c) LLgen.c
LLgen.o:	Lpars.h
LLgen.o:	$(SRC_DIR)/cclass.h
LLgen.o:	$(SRC_DIR)/extern.h
LLgen.o:	$(SRC_DIR)/io.h
LLgen.o:	$(SRC_DIR)/types.h

Lpars.o:	Lpars.c
		$(COMPILE.c) Lpars.c
Lpars.o:	Lpars.h

alloc.o:	$(SRC_DIR)/alloc.c
		$(COMPILE.c) $(SRC_DIR)/alloc.c
alloc.o:	$(SRC_DIR)/extern.h
alloc.o:	$(SRC_DIR)/types.h

cclass.o:	$(SRC_DIR)/cclass.c
		$(COMPILE.c) $(SRC_DIR)/cclass.c
cclass.o:	$(SRC_DIR)/cclass.h

check.o:	$(SRC_DIR)/check.c
		$(COMPILE.c) $(SRC_DIR)/check.c
check.o:	$(SRC_DIR)/extern.h
check.o:	$(SRC_DIR)/io.h
check.o:	$(SRC_DIR)/sets.h
check.o:	$(SRC_DIR)/types.h

compute.o:	$(SRC_DIR)/compute.c
		$(COMPILE.c) $(SRC_DIR)/compute.c
compute.o:	$(SRC_DIR)/extern.h
compute.o:	$(SRC_DIR)/io.h
compute.o:	$(SRC_DIR)/sets.h
compute.o:	$(SRC_DIR)/types.h

gencode.o:	$(SRC_DIR)/gencode.c
		$(COMPILE.c) $(SRC_DIR)/gencode.c
gencode.o:	$(SRC_DIR)/cclass.h
gencode.o:	$(SRC_DIR)/extern.h
gencode.o:	$(SRC_DIR)/io.h
gencode.o:	$(SRC_DIR)/sets.h
gencode.o:	$(SRC_DIR)/types.h

global.o:	$(SRC_DIR)/global.c
		$(COMPILE.c) $(SRC_DIR)/global.c
global.o:	$(SRC_DIR)/extern.h
global.o:	$(SRC_DIR)/io.h
global.o:	$(SRC_DIR)/types.h

machdep.o:	$(SRC_DIR)/machdep.c
		$(COMPILE.c) -DLIBDIR=$(LIBDIRSTR) $(SRC_DIR)/machdep.c
machdep.o:	$(SRC_DIR)/types.h

main.o:	$(SRC_DIR)/main.c
		$(COMPILE.c) $(SRC_DIR)/main.c
main.o:	$(SRC_DIR)/extern.h
main.o:	$(SRC_DIR)/io.h
main.o:	$(SRC_DIR)/sets.h
main.o:	$(SRC_DIR)/types.h

name.o:	$(SRC_DIR)/name.c
		$(COMPILE.c) $(SRC_DIR)/name.c
name.o:	$(SRC_DIR)/extern.h
name.o:	$(SRC_DIR)/io.h
name.o:	$(SRC_DIR)/types.h

reach.o:	$(SRC_DIR)/reach.c
		$(COMPILE.c) $(SRC_DIR)/reach.c
reach.o:	$(SRC_DIR)/extern.h
reach.o:	$(SRC_DIR)/io.h
reach.o:	$(SRC_DIR)/types.h

sets.o:	$(SRC_DIR)/sets.c
		$(COMPILE.c) $(SRC_DIR)/sets.c
sets.o:	$(SRC_DIR)/extern.h
sets.o:	$(SRC_DIR)/sets.h
sets.o:	$(SRC_DIR)/types.h

tokens.o:	tokens.c
		$(COMPILE.c) tokens.c
tokens.o:	Lpars.h
tokens.o:	$(SRC_DIR)/cclass.h
tokens.o:	$(SRC_DIR)/extern.h
tokens.o:	$(SRC_DIR)/io.h
tokens.o:	$(SRC_DIR)/types.h

savegram.o:	$(SRC_DIR)/savegram.c
		$(COMPILE.c) $(SRC_DIR)/savegram.c
savegram.o:	$(SRC_DIR)/types.h
savegram.o:	$(SRC_DIR)/extern.h
savegram.o:	$(SRC_DIR)/io.h
savegram.o:	$(SRC_DIR)/sets.h
