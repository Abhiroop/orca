# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DLOG_PACKET_SIZE=13 -DFAST -mflat
CPPFLAGS	+= -DNETWORK_DEVICE=0
CFLAGS		+= $(SYSTEM_CFLAGS)

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
		pan_mcast_send_rcve.c \
		pan_threads.c

MODULE		:= $(PANDA)/src/system/myrinet/preemptive_threads
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
