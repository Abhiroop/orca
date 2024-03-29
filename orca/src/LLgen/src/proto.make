# $Id: proto.make,v 1.7 1997/02/21 15:44:43 ceriel Exp $

#PARAMS		do not remove this line

SRC_DIR = $(SRC_HOME)/util/LLgen/src
LIBDIR = $(TARGET_HOME)/lib/LLgen
INCLUDES = -I$(SRC_DIR) -I.
LIBDIRSTR = \"$(LIBDIR)\"
DEFINES = -DNDEBUG -DNON_CORRECTING
CFLAGS = $(DEFINES) $(INCLUDES) $(COPTIONS)
LDFLAGS=$(LDOPTIONS)
LINTFLAGS=$(LINTOPTIONS) $(DEFINES) $(INCLUDES) -DNORCSID

LLOPT= # -vvv -x

OBJECTS = main.$(SUF) gencode.$(SUF) compute.$(SUF) LLgen.$(SUF) tokens.$(SUF) \
	check.$(SUF) reach.$(SUF) global.$(SUF) name.$(SUF) sets.$(SUF) \
	Lpars.$(SUF) alloc.$(SUF) machdep.$(SUF) cclass.$(SUF) savegram.$(SUF)
CSRC = $(SRC_DIR)/main.c $(SRC_DIR)/gencode.c $(SRC_DIR)/compute.c \
	$(SRC_DIR)/check.c $(SRC_DIR)/reach.c $(SRC_DIR)/global.c \
	$(SRC_DIR)/name.c $(SRC_DIR)/sets.c $(SRC_DIR)/alloc.c \
	$(SRC_DIR)/machdep.c $(SRC_DIR)/cclass.c $(SRC_DIR)/savegram.c
CFILES = LLgen.c tokens.c Lpars.c $(CSRC)
GFILES = $(SRC_DIR)/tokens.g $(SRC_DIR)/LLgen.g
FILES = $(SRC_DIR)/types.h $(SRC_DIR)/extern.h \
	$(SRC_DIR)/io.h $(SRC_DIR)/sets.h \
	$(GFILES) $(CSRC) $(SRC_DIR)/proto.make

all:		parser
		@make LLgen "LDFLAGS=$(LDFLAGS)" "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LIBDIR=$(LIBDIR)"

parser:		$(GFILES)
		LLgen $(LLOPT) $(GFILES)
		@touch parser

first:		firstparser
		@make LLgen "LDFLAGS=$(LDFLAGS)" "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LIBDIR=$(LIBDIR)"

firstparser:
		cp $(SRC_DIR)/LLgen.c.dist LLgen.c
		cp $(SRC_DIR)/tokens.c.dist tokens.c
		cp $(SRC_DIR)/Lpars.c.dist Lpars.c
		cp $(SRC_DIR)/Lpars.h.dist Lpars.h
		@touch parser

LLgen:		$(OBJECTS)
		$(CC) $(LDFLAGS) $(OBJECTS) $(TARGET_HOME)/modules/lib/libsystem.$(LIBSUF) -o LLgen

pr : 
		@pr $(FILES) $(SRC_HOME)/util/LLgen/lib/rec $(SRC_HOME)/util/LLgen/lib/incl

lint: 		parser
		$(LINT) $(LINTFLAGS) -DLIBDIR=$(LIBDIRSTR) $(CFILES)

clean:
		-rm -f *.$(SUF) LL.temp LL.xxx LL.output LLgen LLgen.c tokens.c Lpars.[ch] parser

distr:
		-rm -f parser
		make parser
		cp Lpars.c $(SRC_DIR)/Lpars.c.dist
		cp Lpars.h $(SRC_DIR)/Lpars.h.dist
		cp LLgen.c $(SRC_DIR)/LLgen.c.dist
		cp tokens.c $(SRC_DIR)/tokens.c.dist

LLgen.$(SUF):	LLgen.c
		$(CC) -c $(CFLAGS) LLgen.c
LLgen.$(SUF):	Lpars.h
LLgen.$(SUF):	$(SRC_DIR)/cclass.h
LLgen.$(SUF):	$(SRC_DIR)/extern.h
LLgen.$(SUF):	$(SRC_DIR)/io.h
LLgen.$(SUF):	$(SRC_DIR)/types.h

Lpars.$(SUF):	Lpars.c
		$(CC) -c $(CFLAGS) Lpars.c
Lpars.$(SUF):	Lpars.h

alloc.$(SUF):	$(SRC_DIR)/alloc.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/alloc.c
alloc.$(SUF):	$(SRC_DIR)/extern.h
alloc.$(SUF):	$(SRC_DIR)/types.h

cclass.$(SUF):	$(SRC_DIR)/cclass.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/cclass.c
cclass.$(SUF):	$(SRC_DIR)/cclass.h

check.$(SUF):	$(SRC_DIR)/check.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/check.c
check.$(SUF):	$(SRC_DIR)/extern.h
check.$(SUF):	$(SRC_DIR)/io.h
check.$(SUF):	$(SRC_DIR)/sets.h
check.$(SUF):	$(SRC_DIR)/types.h

compute.$(SUF):	$(SRC_DIR)/compute.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/compute.c
compute.$(SUF):	$(SRC_DIR)/extern.h
compute.$(SUF):	$(SRC_DIR)/io.h
compute.$(SUF):	$(SRC_DIR)/sets.h
compute.$(SUF):	$(SRC_DIR)/types.h

gencode.$(SUF):	$(SRC_DIR)/gencode.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/gencode.c
gencode.$(SUF):	$(SRC_DIR)/cclass.h
gencode.$(SUF):	$(SRC_DIR)/extern.h
gencode.$(SUF):	$(SRC_DIR)/io.h
gencode.$(SUF):	$(SRC_DIR)/sets.h
gencode.$(SUF):	$(SRC_DIR)/types.h

global.$(SUF):	$(SRC_DIR)/global.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/global.c
global.$(SUF):	$(SRC_DIR)/extern.h
global.$(SUF):	$(SRC_DIR)/io.h
global.$(SUF):	$(SRC_DIR)/types.h

machdep.$(SUF):	$(SRC_DIR)/machdep.c
		$(CC) -c $(CFLAGS) -DUSE_SYS -DLIBDIR=$(LIBDIRSTR) $(SRC_DIR)/machdep.c
machdep.$(SUF):	$(SRC_DIR)/types.h

main.$(SUF):	$(SRC_DIR)/main.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/main.c
main.$(SUF):	$(SRC_DIR)/extern.h
main.$(SUF):	$(SRC_DIR)/io.h
main.$(SUF):	$(SRC_DIR)/sets.h
main.$(SUF):	$(SRC_DIR)/types.h

name.$(SUF):	$(SRC_DIR)/name.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/name.c
name.$(SUF):	$(SRC_DIR)/extern.h
name.$(SUF):	$(SRC_DIR)/io.h
name.$(SUF):	$(SRC_DIR)/types.h

reach.$(SUF):	$(SRC_DIR)/reach.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/reach.c
reach.$(SUF):	$(SRC_DIR)/extern.h
reach.$(SUF):	$(SRC_DIR)/io.h
reach.$(SUF):	$(SRC_DIR)/types.h

sets.$(SUF):	$(SRC_DIR)/sets.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/sets.c
sets.$(SUF):	$(SRC_DIR)/extern.h
sets.$(SUF):	$(SRC_DIR)/sets.h
sets.$(SUF):	$(SRC_DIR)/types.h

tokens.$(SUF):	tokens.c
		$(CC) -c $(CFLAGS) tokens.c
tokens.$(SUF):	Lpars.h
tokens.$(SUF):	$(SRC_DIR)/cclass.h
tokens.$(SUF):	$(SRC_DIR)/extern.h
tokens.$(SUF):	$(SRC_DIR)/io.h
tokens.$(SUF):	$(SRC_DIR)/types.h

savegram.$(SUF):	$(SRC_DIR)/savegram.c
		$(CC) -c $(CFLAGS) $(SRC_DIR)/savegram.c
savegram.$(SUF):	$(SRC_DIR)/types.h
savegram.$(SUF):	$(SRC_DIR)/extern.h
savegram.$(SUF):	$(SRC_DIR)/io.h
savegram.$(SUF):	$(SRC_DIR)/sets.h
