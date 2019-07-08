# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS)
CPPFLAGS	+= -DLOG_PACKET_SIZE=13
CPPFLAGS	+= -DIDLE_THREAD
CPPFLAGS	+= -DFM_NO_INTERRUPTS
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

# gmake has a stupid built-in rule for generating objects from
# assembly files. The rule doesn't use OUTPUT_OPTION.

%.o: %.s
	$(COMPILE.s) $(CPPFLAGS) $<

%.o: %.S
	$(COMPILE.s) $<

SRC		:= \
		pan_sync.c \
		pan_threads.c \
		pan_error.c \
		pan_glocal.c \
		pan_time.c \
		pan_timer.c \
		pan_system.c \
		pan_pset.c \
		pan_nsap.c \
		pan_message.c \
		pan_comm.c \
		pan_malloc.c

MODULE		:= $(PANDA)/src/system/fast_ethernet/amoeba_threads
GENERIC		:= $(PANDA)/src/system/fast_ethernet

include $(PANDA)/include/module.mk
