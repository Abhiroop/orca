PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= mama

SRC_DIR		:= $(PANDA)/tst/src/system/latency

# Additional local flags
CPPFLAGS	+=
CFLAGS		+=
LDFLAGS		+=

SRC		= lat.c \
		  tst.c

OBJ		= $(SRC:%.c=%.o)

lat:		$(OBJ) -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS) 

clean:          
		rm -f Makefile.bak panda_adm *.o *.d lat core

include $(PANDA)/include/appl.mk
