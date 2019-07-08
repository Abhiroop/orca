# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# Add local flags
CPPFLAGS	+= $(BG_CPPFLAGS) -Werror
CFLAGS		+= $(BG_CFLAGS)
 
SRC		:= \
		pan_bg_ack.c \
		pan_bg_alloc.c \
		pan_bg_bb_list.c \
		pan_bg_global.c \
		pan_bg_group.c \
		pan_bg_error.c \
		pan_bg_hist_list.c \
		pan_bg_history.c \
		pan_bg_index.c \
		pan_bg_order_list.c \
		pan_bg_rec.c \
		pan_bg_send.c \
		pan_bg_ticks.c

MODULE		:= $(PANDA)/src/bg

include $(PANDA)/include/module.mk
