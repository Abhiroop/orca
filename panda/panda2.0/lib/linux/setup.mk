CPPFLAGS	+=
CFLAGS		+= 
LDFLAGS		+= 
LIBS		+= -lpthreads

# Always use gcc
CC		:= gcc
LD		:= gcc

DEP		:= $(CC) -MM
