# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.2 1995/07/31 09:14:50 ceriel Exp $

LIBNAM		= flt

include		../../make_paths
include		../../make_params

SRC_DIR		= $(ORCA_HOME)/src/libs/flt_arith
MOD_DIR		= ..

DEFINES		=
INCLUDES	= -I$(SRC_DIR) -I$(MOD_DIR)/h

CPPFLAGS	= $(DEFINES) $(INCLUDES)
CFLAGS		= $(EXT_FLAGS)

CFILES		= flt_ar2flt.c flt_div.c flt_flt2ar.c flt_modf.c flt_str2fl.c \
		  flt_cmp.c  flt_add.c b64_add.c flt_mul.c flt_nrm.c \
		  b64_sft.c flt_umin.c flt_chk.c split.c ucmp.c
CSRC		= $(CFILES:%.c=$(SRC_DIR)/%.c)
OBJECTS		= $(CFILES:%.c=%.o)

LIBFLT		= libflt.a
LIB_OBJECTS	= $(OBJECTS:%.o=$(LIBFLT)(%.o))

all:		$(LIBFLT) # $(LINTPREF)flt.$(LINTSUF)
		-mkdir -p $(MOD_DIR)/h
		/bin/cp $(SRC_DIR)/flt_arith.h $(MOD_DIR)/h/flt_arith.h

$(LIBFLT):	$(LIB_OBJECTS)
		$(AR) $(ARFLAGS) $(LIBFLT) *.o
		/bin/rm -f *.o
		-$(RANLIB) $(LIBFLT)

$(LINTPREF)flt.$(LINTSUF): \
		$(CSRC)
		$(LINT) $(LINTMKLIB) $(CPPFLAGS) $(CSRC)

clean:
		/bin/rm -f *.o *.a

%.o:		$(SRC_DIR)/%.c $(SRC_DIR)/flt_arith.h $(SRC_DIR)/flt_misc.h
		$(COMPILE.c) $(SRC_DIR)/$*.c

$(LIBFLT)(%.o):	$(SRC_DIR)/%.c $(SRC_DIR)/flt_arith.h $(SRC_DIR)/flt_misc.h
		$(COMPILE.c) $(SRC_DIR)/$*.c
