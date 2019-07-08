# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DPACKET_SIZE=7304 \
		   -I$(PANDA)/include/amoeba -DUNIVERSAL_ALIGNMENT=8
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

FILES		:= \
		amoeba/pan_comm.c \
		amoeba/pan_comm.h \
		amoeba/pan_error.c \
		amoeba/pan_error.h \
		generic2.1/pan_fragment.c \
		generic2.1/pan_fragment.h \
		amoeba/pan_global.h \
		amoeba/pan_glocal.c \
		amoeba/pan_glocal.h \
		amoeba/pan_malloc.c \
		generic/pan_malloc.h \
		generic2.1/pan_message.c \
		generic2.1/pan_message.h \
		generic/pan_nsap.c \
		generic/pan_nsap.h \
		generic/pan_pset.c \
		generic/pan_pset.h \
		amoeba/pan_sync.c \
		amoeba/pan_sync.h \
		generic/pan_sys_pool.c \
		generic/pan_sys_pool.h \
		amoeba2.1/pan_system.c \
		generic/pan_system.h \
		amoeba/pan_threads.c \
		amoeba/pan_threads.h \
		amoeba/pan_time.c \
		amoeba/pan_time.h \


include $(PANDA)/include/system.mk
