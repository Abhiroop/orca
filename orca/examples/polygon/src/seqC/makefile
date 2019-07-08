OUT		+= seqCLSDOverlay
OUT		+= seqCLSDNoOutOverlay

## Source.
CFILES		+= main.c
CFILES		+= Box.c

## Compilers.
CC		= gcc
OC_PANDA        = oc_panda


# Flags.
seqCLSDOverlayFLAGS		+=

seqCLSDOverlay.pandaFLAGS	+= -DTIMING

seqCLSDNoOutOverlayFLAGS	+= -DNO_OUTPUT

seqCLSDNoOutOverlay.pandaFLAGS	+= -DNO_OUTPUT
seqCLSDNoOutOverlay.pandaFLAGS	+= -DTIMING

FLAGS		+= -O2

OC_CFLAGS       += -finline-functions
OC_CFLAGS       += -funroll-loops

CFLAGS		+= -ansi
CFLAGS		+= $(FLAGS)
CFLAGS		+= $(OC_CFLAGS)

OC_PANDA_FLAGS  += -amoeba
OC_PANDA_FLAGS  += -2
OC_PANDA_FLAGS  += --O
OC_PANDA_FLAGS  += -v
OC_PANDA_FLAGS  += $(FLAGS)


## Overlay version
unix: $(OUT)
panda: $(OUT:%=%.panda)

$(OUT): \
		$(CFILES) \
		makefile \
		Box.h
	$(CC) $(CFLAGS) $($@FLAGS) $(CFILES) $(LDFLAGS) -o $(@) 

$(OUT:%=%.panda): \
		$(CFILES) \
		makefile \
		Box.h
	$(OC_PANDA) $(OC_PANDA_FLAGS) $($@FLAGS) -o $@ $(CFILES)
	ainstall $@ /home/$@
	rm $@



less:
	rm -f  $(CFILES:%.c=%.d) $(CFILES:%.c=%.o)
	rm -fr .oc_driver


clean:
	rm -f  $(CFILES:%.c=%.d) $(CFILES:%.c=%.o) $(OUT) $(OUT:%=%.panda)
	rm -fr .oc_driver

