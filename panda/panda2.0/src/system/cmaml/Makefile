# Add local flags

# PAN_MAX_SMALL_MSGS

# gmake has a stupid built-in rule for generating objects from
# assembly files. The rule doesn't use OUTPUT_OPTION.

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
# CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

%.o: %.s
	$(COMPILE.s) $(CPPFLAGS) $<

%.o: %.S
	$(COMPILE.s) $<

CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DPACKET_SIZE=1024
CFLAGS		+= $(SYSTEM_CFLAGS)

SRC		:= \
		pan_sync.c \
		pan_threads.c \
		pan_error.c \
		pan_glocal.c \
		pan_time.c \
		pan_system.c \
		pan_pset.c \
		pan_nsap.c \
		pan_message.c \
		pan_comm.c \
		pan_buffer.c \
		pan_fragment.c \
		pan_sys_pool.c \
		pan_malloc.c \
		pan_small.S \
		pan_switch.S \
		gam_cmaml.c \
		malloc.c

MODULE		:= $(PANDA)/src/system/cmaml
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
