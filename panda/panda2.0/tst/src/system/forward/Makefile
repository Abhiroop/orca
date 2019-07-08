PANDA		= /usr/proj/panda/panda2.0
ARCH		= solaris
SYSLIB		= $(PANDA)/lib/$(ARCH)

CC		= gcc

CPPFLAGS	=
CFLAGS		= -I$(PANDA)/include -g -ansi -Wall -pedantic
LDFLAGS		= -L$(SYSLIB)


SRC		= forward.c \
		  tst.c

OBJ		= $(SRC:%.c=%.o)

LIBS		= -lpanda -lsocket -lnsl -lthread -lm

default:	$(OBJ) $(SYSLIB)/libpanda.a
		$(CC) $(LDFLAGS) -o saw $(OBJ) $(LIBS)

depend:		$(SRC)
		makedepend $(CFLAGS) $(SRC)

# DO NOT DELETE THIS LINE -- make depend depends on it.

forward.o: /usr/include/assert.h /usr/include/stdio.h
forward.o: /usr/include/sys/feature_tests.h
forward.o: /usr/proj/panda/panda2.0/include/pan_sys.h forward.h
tst.o: /usr/include/stdio.h /usr/include/sys/feature_tests.h
tst.o: /usr/include/stdlib.h /usr/include/unistd.h /usr/include/sys/types.h
tst.o: /usr/include/sys/machtypes.h /usr/include/sys/unistd.h
tst.o: /usr/proj/panda/panda2.0/include/pan_sys.h forward.h
