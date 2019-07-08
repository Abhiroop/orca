# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.2 1995/07/31 09:14:34 ceriel Exp $

LIBNAM		= alloc

include		../../make_paths
include		../../make_params

SRC_DIR		= $(ORCA_HOME)/src/libs/alloc
MOD_DIR		= ..

DEFINES		=
INCLUDES	= -I$(SRC_DIR) -I$(MOD_DIR)/h

CPPFLAGS	= $(DEFINES) $(INCLUDES)
CFLAGS		= $(EXT_FLAGS)

CFILES		= Malloc.c Salloc.c Srealloc.c Realloc.c botch.c clear.c \
		  st_alloc.c std_alloc.c No_Mem.c
CSRC		= $(CFILES:%.c=$(SRC_DIR)/%.c)
OBJECTS		= $(CFILES:%.c=%.o)

LIBALLOC	= liballoc.a
LIB_OBJECTS	= $(OBJECTS:%.o=$(LIBALLOC)(%.o))

all:		$(LIBALLOC) # $(LINTPREF)alloc.$(LINTSUF)
		-mkdir -p $(MOD_DIR)/h
		/bin/cp $(SRC_DIR)/alloc.h $(MOD_DIR)/h/alloc.h

$(LIBALLOC):	$(LIB_OBJECTS)
		$(AR) $(ARFLAGS) $(LIBALLOC) *.o
		/bin/rm -f *.o
		-$(RANLIB) $(LIBALLOC)

$(LINTPREF)alloc.$(LINTSUF): \
		$(CSRC)
		$(LINT) $(LINTMKLIB) $(CPPFLAGS) $(CSRC)

clean:
		/bin/rm -f *.o *.a

%.o:		$(SRC_DIR)/%.c $(SRC_DIR)/alloc.h
		$(COMPILE.c) $(SRC_DIR)/$*.c

$(LIBALLOC)(%.o): \
		$(SRC_DIR)/%.c $(SRC_DIR)/alloc.h
		$(COMPILE.c) $(SRC_DIR)/$*.c
