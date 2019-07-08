# Add local flags
CPPFLAGS	+= $(SYSTEM_CPPFLAGS) -DUNIVERSAL_ALIGNMENT=4
CFLAGS		+= $(SYSTEM_CFLAGS)

# Remove the -pedantic flag. Luckily, we do not depend on recursive expanding
# after this point.
CFLAGS		:= $(filter-out -pedantic, $(CFLAGS))

FILES		:= \
		parix/pan_bcst_fwd.c \
		parix/pan_bcst_fwd.h \
		parix/pan_bcst_hst.c \
		parix/pan_bcst_hst.h \
		parix/pan_bcst_seq.c \
		parix/pan_bcst_seq.h \
		parix/pan_bcst_snd.c \
		parix/pan_bcst_snd.h \
		parix/pan_comm.c \
		parix/pan_comm.h \
		parix/pan_comm_inf.c \
		parix/pan_comm_inf.h \
		parix/pan_deadlock.c \
		parix/pan_deadlock.h \
		parix/pan_error.c \
		parix/pan_error.h \
		generic2.1/pan_fragment.c \
		generic2.1/pan_fragment.h \
		parix/pan_free_lnk.c \
		parix/pan_free_lnk.h \
		parix/pan_global.h \
		parix/pan_glocal.c \
		parix/pan_glocal.h \
		parix/pan_malloc.c \
		parix/pan_malloc.h \
		generic2.1/pan_message.c \
		generic2.1/pan_message.h \
		parix/pan_msg_cntr.c \
		parix/pan_msg_cntr.h \
		generic/pan_nsap.c \
		generic/pan_nsap.h \
		parix/pan_parix.h \
		generic/pan_pset.c \
		generic/pan_pset.h \
		parix/pan_sync.c \
		parix/pan_sync.h \
		generic/pan_sys_pool.c \
		generic/pan_sys_pool.h \
		parix2.1/pan_system.c \
		parix/pan_system.h \
		parix/pan_threads.c \
		parix/pan_threads.h \
		parix/pan_time.c \
		parix/pan_time.h \
		parix/pan_timer.c \
		parix/pan_timer.h \
		parix/pan_ucast.c \
		parix/pan_ucast.h \


include $(PANDA)/include/system.mk
