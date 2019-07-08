PANDA		:= /usr/proj/panda/panda2.0
ARCH		:= amoeba_reliable/optimized

SRC_DIR		:= $(PANDA)/tst/amoeba_reliable/system/saw

CPPFLAGS	+= \
		-I$(PANDA)/src/system/amoeba \
		-I/usr/proj/amwork/src/h \
		-DPACKET_SIZE=7304

CFLAGS		+=

default:	saw saw_fr

saw:		tst.o saw.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

saw_fr:		tst.o saw_fr.o -lpanda
		$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean
clean:
		-rm -f grp_test* core


include $(PANDA)/include/appl.mk
