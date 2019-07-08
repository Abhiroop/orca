PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= mama/optimized
# ARCH		:= mama

#SRC_DIR		:= $(PANDA)/tst/src/system/threads
SRC_DIR		:= .

# Additional local flags
CPPFLAGS	+= -DNDEBUG
CFLAGS		+= -O2 -finline-functions
LDFLAGS		+=

SRC		= threads.c

OBJ		= $(SRC:%.c=%.o)

threads:	$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:          
		rm -f Makefile.bak panda_adm *.o *.d core

include $(PANDA)/include/appl.mk
