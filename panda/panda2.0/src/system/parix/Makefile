# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DPACKET_SIZE=10000
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
		pan_fragment.c

MODULE		:= $(PANDA)/src/system/generic
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
