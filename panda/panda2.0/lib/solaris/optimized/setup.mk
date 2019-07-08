# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

CPPFLAGS	+= -DNDEBUG
CFLAGS		+= -O2 -finline-functions
LDFLAGS		+=
LIBS		+= -lsocket -lnsl -lthread

# Always use gcc
CC		:= gcc
LD		:= gcc
