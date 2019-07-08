# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.2 1995/07/31 09:14:42 ceriel Exp $

LIBNAM		= system

include		../../make_paths
include		../../make_params

SRC_DIR		= $(ORCA_HOME)/src/libs/system
MOD_DIR		= ..

DEFINES		=
INCLUDES	= -I$(SRC_DIR) -I$(MOD_DIR)/h

CPPFLAGS	= $(DEFINES) $(INCLUDES)
CFLAGS		= $(EXT_FLAGS)

CFILES		= access.c break.c chmode.c close.c create.c filesize.c \
		  modtime.c lock.c open.c read.c remove.c stop.c \
		  system.c time.c unlock.c write.c seek.c rename.c

CSRC		= $(CFILES:%.c=$(SRC_DIR)/%.c)
OBJECTS		= $(CFILES:%.c=%.o)

LIBSYSTEM	= libsystem.a
LIB_OBJECTS	= $(OBJECTS:%.o=$(LIBSYSTEM)(%.o))

all:		$(LIBSYSTEM) # $(LINTPREF)system.$(LINTSUF)
		-mkdir -p $(MOD_DIR)/h
		/bin/cp $(SRC_DIR)/system.h $(MOD_DIR)/h/system.h

$(LIBSYSTEM):	$(LIB_OBJECTS)
		$(AR) $(ARFLAGS) $(LIBSYSTEM) *.o
		/bin/rm -f *.o
		-$(RANLIB) $(LIBSYSTEM)

$(LINTPREF)system.$(LINTSUF): \
		$(CSRC)
		$(LINT) $(LINTMKLIB) $(CPPFLAGS) $(CSRC)

clean:
		/bin/rm -f *.o *.a

%.o:		$(SRC_DIR)/%.c $(SRC_DIR)/system.h
		$(COMPILE.c) $(SRC_DIR)/$*.c

$(LIBSYSTEM)(%.o): \
		$(SRC_DIR)/%.c $(SRC_DIR)/system.h
		$(COMPILE.c) $(SRC_DIR)/$*.c
