# Amoeba directories
AMWORK          := /usr/proj/amwork
AMCONF          := $(AMWORK)/conf/amoeba/sparc.gnu-2
AMBIN           := $(AMWORK)/bin.sun4/gnu
AMINC           := $(AMWORK)/src/h
 
# C compiler
CPPFLAGS        += -I$(PANDA)/include
CPPFLAGS        += -I$(AMINC)
CPPFLAGS        += -I$(AMINC)/server
CPPFLAGS        += -I$(AMINC)/posix
CPPFLAGS        += -I$(AMINC)/toolset/gnu-2
CPPFLAGS        += -Dsparc
CPPFLAGS        += -DAMOEBA
 
CFLAGS          += -mam_sparc
CFLAGS          += -G $(AMBIN)
 
# Avoid generating .d files in current directory
CC              := $(AMCONF)/toolset/do_gcc -nodeps
 
 
LDFLAGS         += -G $(AMBIN)
LDFLAGS         += -mam_sparc
LDFLAGS         += -L$(AMCONF)/lib/math
LDFLAGS         += -L$(AMCONF)/lib/ajax
LDFLAGS         += -L$(AMCONF)/lib/amoeba.mv8
LDFLAGS		+= $(AMCONF)/lib/head/head.o

LIBS		+= $(AMBIN)/sparc/libgcc2.a
LIBS		+= -lmath -lajax -lmath -lamoeba

LD              := $(AMCONF)/toolset/do_ld
 
# do_gcc can't handle -o <file>.o
OUTPUT_OPTION	:= 


