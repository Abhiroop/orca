# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DLOG_PACKET_SIZE=13 -DFM_NO_INTERRUPTS -DIDLE_THREAD
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

SRC		:= \
		pan_comm.c \
		pan_error.c \
		pan_glocal.c \
		pan_malloc.c \
		pan_mcast_global.c \
		pan_mcast_header.c \
		pan_mcast_send_rcve.c \
		pan_mcast_stats.c \
		pan_mcast_sweep.c \
		pan_message.c \
		pan_nsap.c \
		pan_pset.c \
		pan_sync.c \
		pan_system.c \
		pan_threads.c \
		pan_time.c \
		pan_timer.c

MODULE		:= $(PANDA)/src/system/myrinet/amoeba_threads
GENERIC		:= $(PANDA)/src/system/myrinet

include $(PANDA)/include/module.mk
