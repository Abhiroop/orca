# $Id: Makefile,v 1.2 1995/06/13 14:55:04 ceriel Exp $

include		../make_paths
include		../make_params

SRC_DIR		= $(ORCA_HOME)/src/ncpp

CFLAGS		= -I$(SRC_DIR) -DNDEBUG $(EXT_FLAGS)

OFILES		=  \
		LLlex.o \
		LLmessage.o \
		ch3bin.o \
		ch3mon.o \
		domacro.o \
		error.o \
		idf.o \
		init.o \
		input.o \
		main.o \
		options.o \
		preprocess.o \
		replace.o \
		skip.o \
		tokenname.o \
		next.o \
		expr.o \
		tokenfile.o \
		expression.o \
		Lpars.o \
		char.o \
		symbol2str.o \
		Malloc.o \
		Realloc.o \
		Salloc.o \
		close.o \
		doprnt.o \
		fprint.o \
		long2str.o \
		open.o \
		read.o \
		sprint.o \
		st_alloc.o \
		stop.o \
		strrindex.o \
		system.o \
		time.o \
		write.o \
		format.o

ncpp:		$(OFILES)
		$(LINK.c) -o ncpp $(OFILES)

install:	ncpp
		-mkdir -p $(ORCA_TARGET)/bin.$(BINSUF)
		cp ncpp $(ORCA_TARGET)/bin.$(BINSUF)/ncpp

%.o:		$(SRC_DIR)/%.c
		$(COMPILE.c) $(SRC_DIR)/$*.c

clean:
		rm -f *.o ncpp
