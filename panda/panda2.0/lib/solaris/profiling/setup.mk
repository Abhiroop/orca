# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

CPPFLAGS	+=
CFLAGS		+= -O2 -pg
LDFLAGS		+= -pg
LIBS		+= -lsocket -lnsl -lthread -ldl

# Always use gcc
CC		:= gcc
LD		:= gcc
