CPPFLAGS        +=
CFLAGS          += 
LDFLAGS         +=
LIBS            += 
 
# Always use gcc
CC              := px ancc -Tp$(ORCA_HOME)/bin/ancpp-T800 -w
LD              := px ancc -Tp$(ORCA_HOME)/bin/ancpp-T800 -w

DEP		:= gcc -MM
