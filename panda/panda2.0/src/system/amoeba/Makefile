# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DPACKET_SIZE=7304
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

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
		pan_timer.c

MODULE		:= $(PANDA)/src/system/amoeba
GENERIC		:= $(PANDA)/src/system/generic

include $(PANDA)/include/module.mk
