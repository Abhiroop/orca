# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(TRACE_TOOL_CPPFLAGS) -I$(PANDA)/src/trace
CFLAGS		+= $(TRACE_TOOL_CFLAGS)

SRC		:= \
		max_fd.c \
		trc_bind_op_main.c \
		trc_convert.c \
		trc_sm.c \
		trc_filter_main.c \
		trc_state.c \
		states.c \

LDLIBS		+= -lm -lpanda

SRC_DIR		:= $(PANDA)/src/trace/tools

EXECUTABLES	:= max_fd trc_bind_op trc_convert trc_sm trc_filter trc_state

executables:	$(EXECUTABLES)

HOST_ARCH       := $(shell $(PANDA)/bin/arch_ext)
BIN             := $(PANDA)/$(VERSION)/bin.$(HOST_ARCH)

install:	$(EXECUTABLES:%=$(BIN)/%)

$(BIN)/%:	%
		mkdir -p $(BIN)
		cp $* $@

trc_bind_op:	trc_bind_op_main.o -lpanda
		$(LD) $(LDFLAGS) trc_bind_op_main.o -o $@ -lpanda $(LDLIBS)

trc_filter:	trc_filter_main.o -lpanda
		$(LD) $(LDFLAGS) trc_filter_main.o -o $@ -lpanda $(LDLIBS)

trc_convert:	trc_convert.o -lpanda
		$(LD) $(LDFLAGS) trc_convert.o -o $@ -lpanda $(LDLIBS)

trc_sm:		trc_sm.o -lpanda
		$(LD) $(LDFLAGS) trc_sm.o -o $@ -lpanda $(LDLIBS)

trc_state:	trc_state.o states.o -lpanda
		$(LD) $(LDFLAGS) trc_state.o states.o -o $@ -lpanda $(LDLIBS)

.o:		%.o -lpanda
		$(LD) $(LDFLAGS) $*.o -o $@ -lpanda $(LDLIBS)

# max_fd:		max_fd.o $(LDLIBS)
# trc_bind_op_main:	trc_bind_op_main.o $(LDLIBS)
# trc_convert:	trc_convert.o $(LDLIBS)
# trc_sm:		trc_sm.o $(LDLIBS)

include $(PANDA)/include/appl.mk
