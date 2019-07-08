# $Id: Makefile,v 1.2 1995/06/13 14:54:37 ceriel Exp $

include		../make_paths
include		../make_params

CFLAGS		= $(EXT_FLAGS)

all:		mfilter

install:	all
		-mkdir -p $(ORCA_TARGET)/bin.$(BINSUF)
		/bin/cp mfilter $(ORCA_TARGET)/bin.$(BINSUF)/mfilter

trunc_id2.o:	$(ORCA_HOME)/src/mfilter/trunc_id2.c
		$(COMPILE.c) $(ORCA_HOME)/src/mfilter/trunc_id2.c

mfilter:	trunc_id2.o
		$(LINK.c) -o mfilter trunc_id2.o

clean:
		rm -f *.o mfilter
