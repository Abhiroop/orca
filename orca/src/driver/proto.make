# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: proto.make,v 1.10 1996/10/30 14:00:26 ceriel Exp $

# Use this file as a Makefile as well as a template to produce a Makefile from.
#
# ORCA_HOME:	the root of the Orca tree.
#
# make ORCA compiler driver

SRC_DIR		= $(ORCA_HOME)/src/driver
COMP_DIR	= $(ORCA_HOME)/src/comp
LIBDIR		= $(ORCA_HOME)/conf/libs

INCLUDES	= -I. -I$(SRC_DIR) -I$(COMP_DIR) -I$(LIBDIR)/h
DEFINES		=
MACH		= sparc_sunos4
CC		= gcc

OC_HOME		= $(ORCA_HOME)

OC_FLAGS	=
OC_INCLUDES	= -I$$OC_HOME/$$OC_LIBNAM/std
OC_RTSINCLUDES	= -I$$OC_HOME/$$OC_LIBNAM/include \
		  -I$$OC_HOME/$$OC_LIBNAM/include/system/$$OC_RTSNAM
OC_COMP		= $$OC_HOME/$$OC_LIBNAM/oc_c.$$OC_MACH
OC_LIBS		= $$OC_HOME/$$OC_LIBNAM/$$OC_RTSNAM/$$OC_MACH/$$OC_SPECIAL/lib$$OC_RTSNAM.a
OC_CCOMP	= gcc
OC_CFLAGS	= -c
OC_LD		= gcc
OC_LDFLAGS	=
OC_STARTOFF	=
OC_SPECIAL	=
OC_RTSNAM	= unixproc
OC_LIBNAM	= lib
OC_LIBNAMOLD	= lib
CPPFLAGS	= $(DEFINES) \
		  -DOC_HOME='"$(OC_HOME)"' \
		  -DOC_MACH='"$(MACH)"' \
		  -DOC_FLAGS='"$(OC_FLAGS)"' \
		  -DOC_INCLUDES='"$(OC_INCLUDES)"' \
		  -DOC_RTSINCLUDES='"$(OC_RTSINCLUDES)"' \
		  -DOC_COMP='"$(OC_COMP)"' \
		  -DOC_LIBS='"$(OC_LIBS)"' \
		  -DOC_CCOMP='"$(OC_CCOMP)"' \
		  -DOC_CFLAGS='"$(OC_CFLAGS)"' \
		  -DOC_LD='"$(OC_LD)"' \
		  -DOC_LDFLAGS='"$(OC_LDFLAGS)"' \
		  -DOC_STARTOFF='"$(OC_STARTOFF)"' \
		  -DOC_SPECIAL='"$(OC_SPECIAL)"' \
		  -DOC_RTSNAM='"$(OC_RTSNAM)"' \
		  -DOC_LIBNAM='"$(OC_LIBNAM)"' \
		  -DOC_LIBNAMOLD='"$(OC_LIBNAMOLD)"' \
		  $(INCLUDES)
CFLAGS		= $(EXT_FLAGS)
EXT_FLAGS	= -O -ansi -pedantic
MAKEDEPEND	= makedepend

SRC_C		= arglist.c defaults.c idf.c strlist.c chk_compile.c getdb.c \
		  main.c
PWD_C		= $(SRC_C:%.c=$(SRC_DIR)/%.c)
GEN_C		= db.c
CFILES		= $(PWD_C) $(GEN_C)

SRC_H		= arglist.h defaults.h idf.h strlist.h chk_compile.h getdb.h \
		  main.h
PWD_H		= $(SRC_H:%.h=$(SRC_DIR)/%.h)
GEN_H		= locking.h debugcst.h
HFILES		= $(PWD_H) $(GEN_H)

Cfiles:		hfiles $(GEN_C) $(GEN_H) $(SRC_DIR)/proto.make
		touch Cfiles

Makefile.main:	Cfiles
		/bin/cp $(SRC_DIR)/proto.make Makefile.main
		$(MAKEDEPEND) $(CPPFLAGS) -fMakefile.main $(CFILES)

depend:
		/bin/cp $(SRC_DIR)/proto.make Makefile.main
		$(MAKEDEPEND) $(CPPFLAGS) -fMakefile.main $(CFILES)

pr:
		@pr $(SRC_DIR)/proto.make \
			$(PWD_H) \
			$(PWD_C)

clean:
		/bin/rm -f $(GEN_C) $(GEN_G) $(GEN_H) hfiles Cfiles
		/bin/rm -f *.o main

hfiles:		$(SRC_DIR)/Parameters $(COMP_DIR)/make.hfiles
		$(COMP_DIR)/make.hfiles $(SRC_DIR)/Parameters
		touch hfiles

db.c:		$(COMP_DIR)/make.allocd $(COMP_DIR)/db.C
		$(COMP_DIR)/make.allocd < $(COMP_DIR)/db.C > db.c

OBJ		= $(SRC_C:%.c=%.o) $(GEN_C:%.c=%.o)
LIBS		= $(LIBDIR)/alloc/liballoc.a \
		  $(LIBDIR)/system/libsystem.a

LINTLIBS	= $(LIBDIR)/alloc/$(LINTPREF)alloc.$(LINTSUF) \
		  $(LIBDIR)/system/$(LINTPREF)system.$(LINTSUF)

lint:
		$(LINT) $(CPPFLAGS) $(LINTFLAGS) $(C_SRC) $(LINTLIBS)

main:		$(OBJ)
		$(LINK.c) -o main $(OBJ) $(LIBS)

%.o:		$(SRC_DIR)/%.c
		$(COMPILE.c) $(SRC_DIR)/$*.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
