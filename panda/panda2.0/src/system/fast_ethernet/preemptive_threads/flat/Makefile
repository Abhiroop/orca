# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DLOG_PACKET_SIZE=13 -DFAST
CPPFLAGS	+= -DPREEMPTIVE
CPPFLAGS	+= -DCHECK_IN_COND_WAIT
CPPFLAGS	+= -DSKIP_ACTIVE_POLL
CPPFLAGS	+= -DNETWORK_DEVICE=1
CFLAGS		+= $(SYSTEM_CFLAGS) -mflat

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

# gmake has a stupid built-in rule for generating objects from
# assembly files. The rule doesn't use OUTPUT_OPTION.

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
# CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

%.o: %.s
	$(COMPILE.s) $(CPPFLAGS) $<

%.o: %.S
	$(COMPILE.s) $<

SRC		:= \
		pan_comm.c \
		pan_threads.c

MODULE		:= $(PANDA)/src/system/fast_ethernet/preemptive_threads
GENERIC		:= $(PANDA)/src/system/fast_ethernet

include $(PANDA)/include/module.mk
