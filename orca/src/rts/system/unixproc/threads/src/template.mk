# $Id: template.mk,v 1.2 1995/06/28 08:10:12 ceriel Exp $
# This is a make template used to produce a Makefile in the directory in
# which the library is built.

# this module
MODULE		= threads
PWD		= $(RTS_SRC)/system/unixproc/threads/src

# the source files
SRC		= \
		cond.c \
		io.c \
		pthread.c \
		signal.c \
		mutex.c \
		pthread_init.c \
		queue.c \
		stack.c \
		pthread_disp.c

SSRC		= \
		pthread_sched.S

# extra compilation flags for this module
EXTRA_CPPFLAGS	= -DNOERR_CHECK -DC_INTERFACE -DSTACK_CHECK -DSIGNAL_STACK

# include module-independent rules
include $(ORCA_TARGET)/$(VERSION)/module.mk

# module-specific rules
$(OBJ_DIR)/pthread_sched.o: \
		$(PWD)/pthread_sched.S $(FORCE)
		$(COMPILE.c) -o ${OBJ_DIR}/pthread_sched.o $(PWD)/pthread_sched.S
		@-${CHMOD} ${CHMODFLAGS} ${OBJ_DIR}/pthread_sched.o

depend:		pthread_offsets.h

clean:		myclean

myclean:
		-${RM} -f pthread_offsets.h get_offsets

$(LIB_TARGET)(pthread_sched.o): \
		$(PWD)/pthread_sched.S $(FORCE)
		$(COMPILE.c) -o $% $(PWD)/pthread_sched.S
		@-$(AR) $(ARFLAGS) $(LIB_TARGET) $%
		@-$(RM) $%

lib_depend:	pthread_offsets.h

pthread_offsets.h: \
		get_offsets
		get_offsets > pthread_offsets.h

get_offsets:	$(PWD)/get_offsets.c
		$(LINK.c) -o get_offsets $(PWD)/get_offsets.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
