# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DLOG_PACKET_SIZE=13
CPPFLAGS	+= -DNETWORK_DEVICE=1
CPPFLAGS	+= -DPREEMPTIVE
CPPFLAGS	+= -DCHECK_IN_COND_WAIT
CPPFLAGS	+= -DSKIP_ACTIVE_POLL
CPPFLAGS	+= -I$(MODULE) -I$(MODULE)/..
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
		pan_amoeba.c \
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
		pan_malloc.c \
		pan_switch.S

MODULE		:= $(PANDA)/src/system/fast_ethernet/preemptive_threads
GENERIC		:= $(PANDA)/src/system/fast_ethernet

include $(PANDA)/include/module.mk
