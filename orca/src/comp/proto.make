# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: proto.make,v 1.27 1997/07/02 14:12:11 ceriel Exp $

# Use this file as a Makefile as well as a template to produce a Makefile from.
#
# ORCA_HOME:	the root of the Orca tree.
#
# make ORCA compiler

SRC_DIR		= $(ORCA_HOME)/src/comp
LIBDIR		= $(ORCA_HOME)/conf.sparc_sunos4/libs

INCLUDES	= -I. -I$(SRC_DIR) -I$(LIBDIR)/h
DEFINES		=
CC		= gcc
CPPFLAGS	= $(DEFINES) $(INCLUDES)
CFLAGS		= $(EXT_FLAGS)
EXT_FLAGS	= -O -ansi -pedantic -Wall
MAKEDEPEND	= makedepend

LLGEN		= LLgen
LLGENOPTIONS	= -v -n

SRC_G		= program.g declar.g expression.g statement.g
PWD_G		= $(SRC_G:%.g=$(SRC_DIR)/%.g)
GEN_G		= tokenfile.g
GFILES		= $(GEN_G) $(PWD_G)

SRC_C		= LLlex.c LLmessage.c error.c main.c tokenname.c idf.c \
		  input.c type.c def.c misc.c specfile.c node.c const.c \
		  instantiate.c chk.c options.c tokenname2.c generate.c \
		  gen_descrs.c visit.c operation.c process_db.c temps.c \
		  simplify.c gen_code.c gen_expr.c scope.c strategy.c \
		  closure.c Version.c list.c data_flow.c \
		  sets.c opt_SR.c prepare.c bld_graph.c opt_LV.c marshall.c \
		  tpfuncs.c flexarr.c conditional.c

PWD_C		= $(SRC_C:%.c=$(SRC_DIR)/%.c)
GEN_C		= tokenfile.c program.c declar.c expression.c statement.c \
		  symbol2str.c char.c Lpars.c Lncor.c next.c db.c case.c node_num.c
CFILES		= $(PWD_C) $(GEN_C)

SRC_H		= LLlex.h chk.h class.h debug.h f_info.h idf.h input.h \
		  main.h misc.h oc_stds.h tokenname.h options.h error.h \
		  specfile.h instantiate.h generate.h gen_descrs.h visit.h \
		  operation.h db.h process_db.h extra_tokens.h simplify.h \
		  gen_code.h case.h gen_expr.h strategy.h \
		  closure.h node_num.h data_flow.h sets.h opt_SR.h \
		  prepare.h bld_graph.h opt_LV.h marshall.h \
		  tpfuncs.h flexarr.h conditional.h
PWD_H		= $(SRC_H:%.h=$(SRC_DIR)/%.h)
GEN_H		= errout.h idfsize.h numsize.h inputtype.h \
		  locking.h def.h debugcst.h type.h Lpars.h node.h list.h \
		  temps.h scope.h
HFILES		= $(PWD_H) $(GEN_H)

NXHFILES	= def.H type.H node.H list.H temps.H scope.H
NXCFILES	= db.C case.C node_num.C
NEXTFILES	= $(NXHFILES:%.H=$(SRC_DIR)/%.H) \
		  $(NXCFILES:%.C=$(SRC_DIR)/%.C)


Makefile.main:	hfiles LLfiles $(GEN_C) $(GEN_H) $(SRC_DIR)/proto.make
		/bin/cp $(SRC_DIR)/proto.make Makefile.main
		$(MAKEDEPEND) $(CPPFLAGS) -fMakefile.main $(CFILES)

depend:
		/bin/cp $(SRC_DIR)/proto.make Makefile.main
		$(MAKEDEPEND) $(CPPFLAGS) -fMakefile.main $(CFILES)

pr:
		@pr $(SRC_DIR)/proto.make \
			$(SRC_DIR)/Parameters \
			$(SRC_DIR)/char.tab $(PWD_G) $(PWD_H) \
			$(NEXTFILES) $(PWD_C)

clean:
		/bin/rm -f $(GEN_C) $(GEN_G) $(GEN_H) hfiles LLfiles LL.output
		/bin/rm -f *.o main tabgen

LLfiles:	$(GFILES)
		$(LLGEN) $(LLGENOPTIONS) $(GFILES)
		@touch LLfiles
		@if [ -f Lncor.c ] ; then : ; else touch Lncor.c ; fi

hfiles:		$(SRC_DIR)/Parameters $(SRC_DIR)/make.hfiles
		$(SRC_DIR)/make.hfiles $(SRC_DIR)/Parameters
		touch hfiles

tokenfile.g:	$(SRC_DIR)/tokenname.c $(SRC_DIR)/make.tokfile
		$(SRC_DIR)/make.tokfile <$(SRC_DIR)/tokenname.c >tokenfile.g

symbol2str.c:	$(SRC_DIR)/tokenname.c $(SRC_DIR)/make.tokcase
		$(SRC_DIR)/make.tokcase <$(SRC_DIR)/tokenname.c >symbol2str.c

def.h:		$(SRC_DIR)/make.allocd $(SRC_DIR)/def.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/def.H > def.h

temps.h:	$(SRC_DIR)/make.allocd $(SRC_DIR)/temps.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/temps.H > temps.h

type.h:		$(SRC_DIR)/make.allocd $(SRC_DIR)/type.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/type.H > type.h

node.h:		$(SRC_DIR)/make.allocd $(SRC_DIR)/node.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/node.H > node.h

list.h:		$(SRC_DIR)/make.allocd $(SRC_DIR)/list.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/list.H > list.h

scope.h:	$(SRC_DIR)/make.allocd $(SRC_DIR)/scope.H
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/scope.H > scope.h

db.c:		$(SRC_DIR)/make.allocd $(SRC_DIR)/db.C
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/db.C > db.c

case.c:		$(SRC_DIR)/make.allocd $(SRC_DIR)/case.C
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/case.C > case.c

node_num.c:	$(SRC_DIR)/make.allocd $(SRC_DIR)/node_num.C
		$(SRC_DIR)/make.allocd < $(SRC_DIR)/node_num.C > node_num.c

next.c:		$(NEXTFILES) $(SRC_DIR)/make.next
		$(SRC_DIR)/make.next $(NEXTFILES) > next.c

char.c:		$(SRC_DIR)/char.tab tabgen
		./tabgen -f$(SRC_DIR)/char.tab >char.c

tabgen:		$(SRC_DIR)/tabgen.c
		$(LINK.c) -o tabgen $(SRC_DIR)/tabgen.c

OBJ		= $(SRC_C:%.c=%.o) $(GEN_C:%.c=%.o)
LIBS		= $(LIBDIR)/flt_arith/libflt.a \
		  $(LIBDIR)/alloc/liballoc.a \
		  $(LIBDIR)/system/libsystem.a

LINTLIBS	= $(LIBDIR)/flt_arith/$(LINTPREF)flt.$(LINTSUF) \
		  $(LIBDIR)/alloc/$(LINTPREF)alloc.$(LINTSUF) \
		  $(LIBDIR)/system/$(LINTPREF)system.$(LINTSUF)

lint:
		$(LINT) $(CPPFLAGS) $(LINT_FLAGS) $(CFILES) $(LINTLIBS)

main:		$(OBJ)
		$(LINK.c) -o main $(OBJ) $(LIBS)

%.o:		$(SRC_DIR)/%.c
		$(COMPILE.c) $(SRC_DIR)/$*.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
