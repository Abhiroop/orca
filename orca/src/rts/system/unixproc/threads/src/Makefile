#  Copyright (C) 1992, the Florida State University
#  Distributed by the Florida State University under the terms of the
#  GNU Library General Public License.
#
#This file is part of Pthreads.
#
#Pthreads is free software; you can redistribute it and/or
#modify it under the terms of the GNU Library General Public
#License as published by the Free Software Foundation (version 2).
#
#Pthreads is distributed "AS IS" in the hope that it will be
#useful, but WITHOUT ANY WARRANTY; without even the implied
#warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#See the GNU Library General Public License for more details.
#
#You should have received a copy of the GNU Library General Public
#License along with Pthreads; see the file COPYING.  If not, write
#to the Free Software Foundation, 675 Mass Ave, Cambridge,
#MA 02139, USA.
#
#Report problems and direct all questions to:
#
#  pthreads-bugs@ada.cs.fsu.edu
#
#   @(#)Makefile	1.20 10/1/93
#

#Compile flags:
#C_INTERFACE:		for C-language interface; if this option is omitted,
#			a language-independent interface will be generated
#			which has been tested for Verdix/SunAda.
#C_CONTEXT_SWITCH:	for context switch written in C. Notice that the
#			assembly version for the SPARC is faster but by far
#			less portable (see portability notes in pthread_disp.c
#			and pthread_sched.S).
#			Internal signal handling (pthread_kill) is also much
#			slower with the context switch in C, about a factor 2.
#			speed(C context switch) =~ 1.2*speed(as context switch)
#CLEANUP_HEAP:		allocate cleanup handlers on heap (default: stack).
#DEBUG:			dump out trace information (mainly for signals).
#DEF_RR:		supports round-robin time-sliced scheduling
#DRAFT5:		defines pthread_setprio() interface bypassing attributes
#IO:			I/O operations only block current thread, not process.
#			(SunOS kernel configuration must include light-weight
#			processes [LWP] and aynchronous I/O [AIO] if this
#			option is set.)
#MALLOC:		thread-safe malloc, in conjuction with Gnu malloc lib.
#			See README for more information.
#NOERR_CHECK:		fast mutex operations without error checking.
#NO_INLINE:		no inlines for macros (internally CLEAR_KERNEL_FLAG).
#MUT_SWITCH:		forces context switch upon successful mutex_lock()
#			with regard to priorities (do not define NOERR_CHECK)
#RAND_SWITCH:		forces context switch on kernel exit at random intervals
#			and to random threads without regard to priorities
#			(do not define NOERR_CHECK)
#RR_SWITCH:		forces context switch on kernel exit (round-robin queue)
#			without regard to priorities (do not define NOERR_CHECK)
#SIGNAL_STACK:		Allows user to execute handlers in case of a stack
#			overflow. STACK_CHECK has to be on for this.
#SIM_KERNEL:		simulate kernel implementation by using dummy UNIX
#			system call in selected routines.
#SRP:			support of mutex ceiling under stack resource
#			policy (SRP)
#			if not defined, _POSIX_THREADS_PRIO_PROTECT has to be
#			undefined in unistd.h!
#STACK_CHECK:		lock page to cause bus error / illegal instruction
#			on stack overflow
#TIMER_DEBUG:		dump out trace information for SIGALRM signals and timer


#for shipping
#CFLAGS = -DSRP -DNOERR_CHECK -DC_INTERFACE -DSTACK_CHECK -DSIGNAL_STACK -DIO
#for maximum portability (minimum assembly required)
#CFLAGS = -DSRP -DCLEANUP_HEAP -DC_INTERFACE -DSTACK_CHECK -DSIGNAL_STACK -DIO \
	-DC_CONTEXT_SWITCH
#for thread-safe malloc
#CFLAGS = -DSRP -DNOERR_CHECK -DC_INTERFACE -DSTACK_CHECK -DSIGNAL_STACK -DIO \
	-DMALLOC
#for PART project
#CFLAGS = -DSRP -DNOERR_CHECK -DSTACK_CHECK -DSIGNAL_STACK -DDRAFT5
#for testing
#CFLAGS = -DDEBUG -DTIMER_DEBUG -DSRP -DNOERR_CHECK -DC_INTERFACE

#for shipping
#CCFLAGS = -O3 $(CFLAGS)
#CCFLAGS2 = -O2 $(CFLAGS)
#for testing
CCFLAGS = -g -O2 $(CFLAGS)
CCFLAGS2 = $(CCFLAGS)

#for gnu C compiler
CC = gcc

LINTFLAGS = -u $(CFLAGS)

LIBS = ../lib/libpthreads.a

CSRC = pthread.c stack.c mutex.c cond.c pthread_init.c signal.c \
	cond.c queue.c io.c pthread_disp.c

SCRS = $(CSRC) pthread_sched.S

OBJS = pthread.o stack.o mutex.o cond.o pthread_init.o \
	signal.o pthread_sched.o queue.o io.o pthread_disp.o

HDR1 = pthread_internals.h pthread.h unistd.h limits.h errno.h \
	signal.h pthread_asm.h setjmp.h

DEP1 = $(HDR1) Makefile

HDRS = $(HDR1) pthread_offsets.h mutex.h signal_internals.h

COMP_C = $(CC) $(CCFLAGS) -c $*.c
COMP_C2 = $(CC) $(CCFLAGS2) -c $*.c
COMP_ASM = $(AS) -P $(CFLAGS) $*.S -o $*.o

all: $(LIBS)

$(LIBS):: $(OBJS)
	ar ru $(LIBS) *.o
	ranlib $(LIBS)

cond.o: $(DEP1) mutex.h
	$(COMP_C)

mutex.o: $(DEP1) mutex.h
	$(COMP_C)

pthread.o: $(DEP1)
	$(COMP_C)

pthread_init.o: $(DEP1)
	$(COMP_C)

signal.o: $(DEP1) mutex.h signal_internals.h
	$(COMP_C)

stack.o: $(DEP1)
	$(COMP_C)

queue.o: $(DEP1)
	$(COMP_C)

io.o: $(DEP1)
	$(COMP_C)

pthread_disp.o: $(DEP1)
	$(COMP_C2)

pthread_sched.o: pthread_sched.S pthread_asm.h pthread_offsets.h Makefile
	$(COMP_ASM)

pthread_offsets.h: get_offsets
	get_offsets > pthread_offsets.h

get_offsets: get_offsets.c $(DEP1) setjmp.h
	$(CC) $(CCFLAGS) -o $@ $@.c

lint:
	lint $(LINTFLAGS) $(CSRC)

llib:
	lint $(LINTFLAGS) -Cthreads $(SRCS)

clean:
	-/bin/rm -f $(LIBS) a.out core errs *.o *.BAK *.CKP *~ #*
