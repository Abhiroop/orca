# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DPACKET_SIZE=7304
CFLAGS		+= $(SYSTEM_CFLAGS)

SRC		:= \
		pan_buffer.c \
		pan_comm.c \
		pan_error.c \
		pan_glocal.c \
		pan_malloc.c \
		pan_message.c \
		pan_nsap.c \
		pan_pset.c \
		pan_sync.c \
		pan_sys_pool.c \
		pan_system.c \
		pan_threads.c \
		pan_time.c

MODULE		:= $(PANDA)/src/system/amoeba_wrapper
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
