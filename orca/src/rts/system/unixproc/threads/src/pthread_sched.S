/* Copyright (C) 1992, the Florida State University
   Distributed by the Florida State University under the terms of the
   GNU Library General Public License.

This file is part of Pthreads.

Pthreads is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation (version 2).

Pthreads is distributed "AS IS" in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with Pthreads; see the file COPYING.  If not, write
to the Free Software Foundation, 675 Mass Ave, Cambridge,
MA 02139, USA.

Report problems and direct all questions to:

  pthreads-bugs@ada.cs.fsu.edu

  @(#)pthread_sched.S	1.20 10/1/93

*/

#ifdef C_CONTEXT_SWITCH
/*
 * Context switch supporting assembly routines and other assembly routines
 *
 * Portability notes:
 * There still is assembly, even with the "C context switch":
 *   .service routines such as set/get PC, SP, FP etc.
 *   .routines to provide proper entry points for fake calls (parameter
 *    passing on stack etc. is architecture-dependent).
 *    This is the tricky part, but just about 30 lines.
 *   .setjmp/longjmp has to be rewritten to work with the thread structure.
 *   OPTIONAL:
 *   .cleanup handlers manipulate the stack. There is a slow version
 *    using the heap (-DCLEANUP_HEAP) which requires no assembly.
 *   .fast mutex locking is still in assembly, but the slower version is
 *    in C (do NOT defined -DNOERR_CHECK).
 */
#else
/*
 * Context switch supporting assembly routines and other assembly routines
 */
#endif

#include <sys/errno.h>
#include <sun4/trap.h>
#define KERNEL
#include <sun4/asm_linkage.h>
#include "unistd.h"
#undef KERNEL
#include "pthread_asm.h"
#include "pthread_offsets.h"
#define LOCORE
#include <signal.h>
#undef LOCORE

	.seg	"text"

#ifndef C_CONTEXT_SWITCH
!	Temporary stack space

	.seg	"bss"			! Could also use data segment
	.skip	SA(MINFRAME)		! Spare window in case lower windows
					!   take up more space (e.g. locals)
	.skip	SA(MINFRAME)		! Universal handler window
	.skip	SA(MINFRAME)		! UNIX sigtramp window
#ifdef DEBUG
	.skip	100*SA(MINFRAME)	! Calls by handle_many_process_signals
#else
	.skip	50*SA(MINFRAME)		! Calls by handle_many_process_signals
#endif
	.skip	SA(MINFRAME)		! Handle_many_process_signals window
	.skip	SA(WINDOWSIZE)		! Dispatcher window
	.align	STACK_ALIGN
#if STACK_CHECK && SIGNAL_STACK
	.global	_pthread_tempstack
#endif
_pthread_tempstack:			! Stack base for dispatcher
	.skip	SA(WINDOWSIZE)		! Empty previous frame (callee save)

	.seg	"text"

!	void pthread_sched
!
        .globl  _pthread_sched
_pthread_sched:
	set	_pthread_kern,%g1	! Get kernel address
#ifdef NO_INLINE
	st	%g0,[%g1+is_in_kernel]	! Clear kernel flag
	ld	[%g1+state_change],%g2	! Get kernel state
	tst	%g2			! check if state changed
	bne	state_changed
	nop
	retl
	nop
state_changed:
#endif
	st	%i7,[%g1+is_in_kernel]	! Enter kernel again
	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window
pthread_sched_no_save:
	ld	[%g1+pthread_self],%l0	! Get current thread
	sethi	%hi(_errno),%l5		! Get high addr of process errno
	ld	[%g1+ready_head],%i0	! Load new thread

	ld	[%l0+state],%l2		! Get the current context state
	btst	T_RETURNED,%l2		! Returned?
	be	test_old		! No, save old state and restore new
	ld	[%g1+new_signals],%l4	! Delay: Load pending signals
	ta	ST_FLUSH_WINDOWS	! Flush all windows in current context
	btst	T_DETACHED,%l2		! Both returned and detached?
	be	test_new		! No, restore the new state
	tst	%l4			! Delay: Test if signals pending
	mov	%g1,%l3			! Delay: Do not lose address of kernel

! Free the stack of the current context.
! Freeing the stack we are operating on seems to be save as long as we are
! not preempted while doing so. Since there is no preemption within pthreads,
! we do not care to guard this section of code. Using a temporary stack
! resulted in some problems because it is not known how much space should
! be allocated for the calls to _free in addition to the save area for
! the register windows.

dealloc:
#ifdef MALLOC
	call	_pthread_free
#else !MALLOC
	call	_free
#endif MALLOC
	ld	[%l0+stack_base],%o0	! Delay slot - Free the stack

! Free the thread structure for the current thread.  After this, we must be
! careful not to store a context into it.
#ifdef MALLOC
	call	_pthread_free
#else !MALLOC
	call	_free
#endif MALLOC
	mov	%l0,%o0			! Delay slot
	mov	%g0, %l0		! Denotes thread has been freed

	mov	%l3, %g1		! Do not lose address of kernel
	b	test_new		! Skip save
	tst	%l4			! Delay: Test if signals pending
test_old:
	cmp	%l0,%i0
	bne	save_old		! Jump if new thread on head of ready
	tst	%l4			! Delay: Test if signals pending
	be	no_switch		! Jump if no pending signals
					! Delay: Load high interrupt ret addr
        sethi	%hi(_called_from_sighandler),%g2
save_old:
	ta	ST_FLUSH_WINDOWS	! Flush all windows in current context

	ld	[%l5+%lo(_errno)],%l6	! Get errno
	st	%l6,[%l0+thread_errno]	! Save errno of current thread
	std	%fp,[%l0+sp_offset]	! Save the current stack pointer
!impl.	st	%i7,[%l0+pc_offset]	! Save the current return pc
test_new:
					! Jump if signals pending
	bne	handle_pending_signals
	tst	%i0
	be	no_threads		! Jump if no thread on ready queue
	.empty				! Label Ok in delay slot
restore_new:
					! (Delay:) Load high interrupt ret addr
        sethi	%hi(_called_from_sighandler),%g2
	ld	[%i0+thread_errno],%l6	! Errno of new thread
	st	%l6,[%l5+%lo(_errno)]	! Save errno
!impl.	ld	[%i0+pc_offset],%i7	! Load the new pc
        ldd     [%i0+sp_offset],%fp     ! Load the new stack pointer
no_switch:
#ifdef DEF_RR
	ld	[%i0+sched],%l0		! Load schedular attribute
 	cmp	%l0,SCHED_RR		! Is it RR (round-robin) ?
 	bne	no_rr			! Do not set timer for RR
					! Delay: Load low interrupt ret address
        or	%g2,%lo(_called_from_sighandler),%g2
	mov	%g1,%l3			! Do not lose the address of kernel
 	mov	%i0,%o0			! Move parameter - thread_t
 	call	_pthread_timed_sigwait	! Set the timer for RR
 	mov	RR_TIME,%o2		! Move parameter - Mode
	mov	%l3,%g1			! Do not lose the address of kernel
                                        ! Load high interrupt return addr
        sethi	%hi(_called_from_sighandler),%g2
#endif
					! Load low interrupt return address
        or	%g2,%lo(_called_from_sighandler),%g2
#ifdef DEF_RR
no_rr:
#endif
	cmp	%g2,%i7			! Compare pc-map with new pc
	bne	skip_sig1		! Jump if mask not interrupt return
	st	%i0,[%g1+pthread_self]	! Delay: Store new thread
	mov	%g1, %l3		! Do not lose address of kernel
	call	_sigsetmask		! Disable signals (parameter mask)
	ld	[%g1+S_DISABLE],%o0	! Delay : load parameter
	mov	%l3, %g1		! Do not lose address of kernel


! Return into the new context.  Since all windows have been flushed, the
! RESTORE will cause a window underflow trap, restoring the registers of the
! new context from its stack.
skip_sig1:
	restore
	st	%g0,[%g1+state_change]	! Clear state_change flag
	st	%g0,[%g1+is_in_kernel]	! Clear kernel flag

	ld	[%g1+new_signals],%g3
	tst	%g3			! Any signals received while in kernel?
	bne	sig_pending
	nop

	retl				! Activate new thread
	nop
sig_pending:
	st	%i7,[%g1+is_in_kernel]	! Enter kernel again
	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window

	ld	[%g1+pthread_self],%l0	! Get current thread
	sethi	%hi(_errno),%l5		! Get (high) address of process errno

	ta	ST_FLUSH_WINDOWS	! Flush all windows in current context

	ld	[%l5+%lo(_errno)],%l6	! Get errno
	st	%l6,[%l0+thread_errno]	! Save errno of current thread
	std	%fp,[%l0+sp_offset]	! Save the current stack pointer
!impl.	st	%i7,[%l0+pc_offset]	! Save the current return pc
                                        ! Load interrupt return address
        set     _called_from_sighandler,%g2

	cmp	%g2,%i7			! Compare pc-map w/ new pc again
	bne	skip_sig2		! Jump if mask not interrupt return

	mov	%g1,%l3			! Delay: Do not lose address of kernel
	call	_sigsetmask		! Enable signals
	ld	[%g1+S_ENABLE],%o0	! Delay: Load mask
	ba,a	skip_sig2		! Ignore Delay: Jump always

handle_pending_signals:
no_threads:
	mov     %g1,%l3			! Do not lose address of kernel

skip_sig2:
					! Get temp. stack address
	sethi	%hi(_pthread_tempstack-SA(WINDOWSIZE)),%l1
	call	_pthread_handle_many_process_signals
	add	%l1,%lo(_pthread_tempstack-SA(WINDOWSIZE)),%sp
	cmp	%g0,%l0			! test if deallocated
	bne	restore_stack		! No
	nop				! Delay:
	mov	%o0,%l0			! Copy return Val (ready.head) to %l0
restore_stack:
	ld	[%l0+sp_offset],%l2	! Switch back to stack of thread
	add	%l2,-SA(WINDOWSIZE),%sp
	mov	%o0,%i0			! Move ret val from call into i0

	cmp	%l0,%i0			! Thread at the head of ready?
	be	restore_new		! Jump if so
	mov	%l3, %g1		! Do not lose address of kernel
	ld	[%l0+state],%l2		! Get the current context state
	btst	T_RETURNED,%l2		! Returned?
	be	restore_new		! No, restore the new state
	btst	T_DETACHED,%l2		! Delay: both returned and detached?
	be	restore_new		! No, restore the new state
	nop
	b	dealloc			! Free structures
	ld      [%g1+new_signals],%l4   ! Delay: Load pending signals

#endif C_CONTEXT_SWITCH

#ifdef IO
!	The following is intentionally dead code. It ensures that the new
!	versions of read and write are always linked in.

	call	_read			! dead code
	nop
	call	_write			! dead code
	nop
#endif

#ifdef C_CONTEXT_SWITCH
!	void pthread_sched_wrapper(sig, code, p)
!	int sig;
!	int code;
!	pthread_t p;
#else
!	void pthread_sched_wrapper(sig, code)
!	int sig;
!	int code;
#endif
!
!	wrapper for pthread_sched() to determine the pc
!	called from the sighandler()
!	calls pthread_sched() and provides a global address for this call
!	which can be used by pthread_sched() to determine if it is about
!	to switch context to an interrupted thread (by comparing the
!	return value with the global address _called_from_sighandler.
        .globl  _called_from_sighandler
        .globl  _pthread_sched_wrapper
_pthread_sched_wrapper:
	save	%sp,-SA(MINFRAME),%sp	! Get a new window.

#ifdef C_CONTEXT_SWITCH
	mov	%i2,%o0			! Context structure of pthread_self()
_called_from_sighandler:		! Address of call instruction
	call	_sigsetjmp
	clr	%o1			! Delay: parameter 2

	tst	%o0			! Check return value
	bne	is_longjmp
#endif
	mov	%i1, %o1		! Delay: Pass parameter 2
#ifndef C_CONTEXT_SWITCH
_called_from_sighandler:		! Address of call instruction
#endif
	call	_pthread_signal_sched	! Call other dispatcher (handle signal)
	mov	%i0, %o0		! Delay: Pass parameter 1
is_longjmp:
	ret
	restore


!	int pthread_not_called_from_sighandler(addr)
!	int addr;
!
!	returns FALSE iff the address passed in corresponds to the sighandler
	.global _pthread_not_called_from_sighandler
_pthread_not_called_from_sighandler:
	set	_called_from_sighandler,%o1
	retl
	sub	%o0,%o1,%o0		! Delay: return difference of param
					! and sighandler return address

!	int pthread_test_and_set(flag)
!	int *flag;
!
!	executes a test & set instruction on the passed flag and
!	returns the previous value of the flag. (leaf routine)
	.global _pthread_test_and_set
_pthread_test_and_set:
	retl
	ldstub	[%o0],%o0		! Delay: Flag in o0 (no save/restore)

!	char *pthread_get_sp()
!
!	returns the current sp (leaf procedure)
	.global _pthread_get_sp
_pthread_get_sp:
	retl
	mov	%sp, %o0		! Delay: sp in o0 (no save/restore)

!       void pthread_set_sp(new_sp)
!	int *new_sp;
!
!       sets the sp (leaf procedure)
        .global _pthread_set_sp
_pthread_set_sp: 
        retl
        mov      %o0, %sp             ! Delay: o0 in sp (no save/restore)

!       char *pthread_get_fp()
!
!       returns the current fp (leaf procedure)
        .global _pthread_get_fp   
_pthread_get_fp: 
        retl
        mov      %fp, %o0             ! Delay: fp in o0 (no save/restore)
 
!       void pthread_set_fp(new_fp) 
!	int *new_fp;
!
!       sets the fp (leaf procedure) 
        .global _pthread_set_fp   
_pthread_set_fp:  
        retl 
        mov      %o0, %fp             ! Delay: o0 in fp (no save/restore) 

!	void pthread_ST_FLUSH_WINDOWS()
!
!	Flushes the windows
	.global	_pthread_ST_FLUSH_WINDOWS
_pthread_ST_FLUSH_WINDOWS:
	retl
	ta	ST_FLUSH_WINDOWS	! Delay:Flush Windows(no save/restore)

#ifdef C_CONTEXT_SWITCH
!	void pthread_fake_call_wrapper_wrapper(user_handler, smask, sig, infop,
!					       scp, restore_context, oscp, cond)
!	void (*user_handler)();
!	sigset_t smask;
!	int sig;
!	struct siginfo *infop;
!	struct sigcontext *scp;
!	int restore_context;
!	struct sigcontext *oscp;
!	pthread_cond_t **cond;
!
!	Provides correct entry point to pthread_fake_call_wrapper() WITHOUT
!	save instruction (no new register window). Simply pass the parameters.
!	Notice that we never return from the call since pthread_sched() will
!	transfer the control-flow.
	.global	_pthread_fake_call_wrapper_wrapper
_pthread_fake_call_wrapper_wrapper:
	mov	%i0,%o0
	mov	%i1,%o1
	mov	%i2,%o2
	mov	%i3,%o3
	mov	%i4,%o4
	call	_pthread_fake_call_wrapper
	mov	%i5,%o5
	ret
	restore

!	void pthread_clear_kernel_flag_wrapper_wrapper
!
!	Provides correct entry point to pthread_clear_kernel_flag_wrapper()
!	without	save instruction (no new register window). The restore
!	instruction in the delay slot ensures that the actual wrapper
!	is executed in the same window as the wrapper_wrapper, thereby
!	minimizing stack usage to one frame.
!	Notice that we never return from the call since the wrapper_wrapper
!	will return to the previous frame.
	.global	_pthread_clear_kernel_flag_wrapper_wrapper
_pthread_clear_kernel_flag_wrapper_wrapper:
	b	_pthread_clear_kernel_flag_wrapper
	restore
! 	Should never reach here

#else !C_CONTEXT_SWITCH

!	void pthread_fake_call_wrapper(user_handler, sig_mask, sig, infop, scp,
!			       restore_context, oscp, cond)
!	void (*user_handler)();
!	sigset_t sig_mask;
!	int sig;
!	struct siginfo *infop;
!	struct sigcontext *scp, *oscp;
!	int new_context;
!	pthread_cond_t *cond;
!
!       If the conditional variable pointer is non-null, the pending
!	conditional wait terminates and the mutex is relocked
!	before the user handler is called. This is only done once for
!	nested handlers by the innermost handler (see check for zero-value
!	of the condition variable).
!	Then the user handler is called with parameters sig, infop, scp.
!	The errno is saved across the user handler call.

!	Notice that the frame of this wrapper is already put the
!	stack by fake_call, therefore, we should NOT call "save" to
!	get a new window.
!	Notice that the address of the condition variable is
!       passed(!) in %l6 if the signal came in during a conditional wait.
!	Notice that oscp is passed(!) in %l7 and is restored as p->nscp
!	upon return from the wrapper.
	.global _pthread_fake_call_wrapper
_pthread_fake_call_wrapper:
	sethi	%hi(_errno),%l0	! Get process errno (high)
	ld	[%l6],%l6	! Get condition variable
	tst	%l6		! Cond. var. == 0 ?
	be	no_cond_wait
				! Delay: Get process errno (low)
	ld	[%l0+%lo(_errno)],%l3
				! Terminate cond. wait
	call	_pthread_cond_wait_terminate
	.empty				! Label Ok in delay slot
no_cond_wait:
				! Delay: get old process mask
	ld	[%i4+sc_mask],%l2

	mov	%i2, %o0	! Load param0 to user-defined handler
	mov	%i3, %o1	! Load param1 to user-defined handler
	call	%i0		! Call a user-defined handler
	mov	%i4, %o2	! Load param2 to user-defined handler

				! Get base address of kernel structure
	set	_pthread_kern,%g1
				! Enter kernel again
	st	%i7,[%g1+is_in_kernel]
	ld	[%g1+pthread_self],%i3
	tst	%i5		! Check if context needs to be restored
	be	UNIX_restore
				! Delay: Restore errno
	st	%l3,[%l0+%lo(_errno)]
				! Load signal masked to be restored
	ld	[%i4+sc_mask],%i1
	ld	[%i4+sc_sp],%fp	! Load stack pointer to be restored
	ld	[%i4+sc_pc],%i7	! Load program counter to be restored
				! Substract offset added by ret instruction
	add	%i7,-RETURN_OFFSET,%i7
	ba	context_restored
				! Delay: Load pending signals
	ld	[%g1+pending_signals],%l4

UNIX_restore:			! Signal mask applies to thread, not process
	ld	[%i4+sc_mask],%l3
	cmp	%l3,%l2
	be	context_restored
				! Delay: Load pending signals
	ld	[%g1+pending_signals],%l4
	mov	%l3, %i1	! Apply context signal mask on thread
				! Restore original signal mask for UNIX
	st	%l2,[%i4+sc_mask]

context_restored:
	st	%i1,[%i3+mask]	! Store new per-thread mask
				! Load pending signals on thread
	ld	[%i3+pending],%l1
	or	%l1,%l4,%l1	! Bit or with pending signals on process
	andncc	%l1,%i1,%g0	! Any pending signal cleared ?
				! If branch, leave kernel
				! but stick with current frame
        be      not_pending
	nop
				! Handle signals pending on thread
	call	_pthread_handle_pending_signals_wrapper
	mov	0, %o0		! Delay: parameter no initial save
				! Should never return to this point
not_pending:                    ! Leave kernel but stick with current frame
        ba      pthread_sched_no_save
	st	%l7,[%i3+nscp]	! Delay: Copy prev sigcontext into current one	

!	void pthread_handle_pending_signals_wrapper(initial_save)
!	int initiual_save;
!
!	change to temp stack and call handle_pending_signals()
!	then jumps into regular scheduler
!	creates own frame at beginning or after save thread context
!	depending on initial_save in %i0/%o0
!	assumes SET_KERNEL_FLAG

	.global _pthread_handle_pending_signals_wrapper
_pthread_handle_pending_signals_wrapper:
				! Get the kernel address
	set	_pthread_kern,%g3
	tst	%o0
	be	no_initial_save
	ld	[%g3+pthread_self],%g4	! Delay: get current thread

	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window
no_initial_save:
	sethi	%hi(_errno),%g1		! Save the thread context
	ld	[%g1+%lo(_errno)],%g2	! Get errno
	st	%g2,[%g4+thread_errno]	! Save errno of current thread
	std	%fp,[%g4+sp_offset]	! Save the current stack pointer
!impl.	st	%i7,[%g4+pc_offset]	! Save the current return pc

	bne	initial_save
					! Delay: load temp. stack address
	sethi	%hi(_pthread_tempstack-SA(WINDOWSIZE)),%g2

	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window
initial_save:
	ta	ST_FLUSH_WINDOWS	! Flush all windows after init save
	mov	%g3, %l3		! Save kernel address
	mov	%g4, %l0		! Save pthread_self
	mov	%g1, %l5		! Save errno address

	call	_pthread_handle_pending_signals
					! Delay: switch to _tempstack
	add	%g2,%lo(_pthread_tempstack-SA(WINDOWSIZE)),%sp

	ba	restore_stack
	ld	[%l3+ready_head],%o0		! Delay: parameter ready.head



!	void pthread_signal_sched(sig, code)
!	int sig;
!	int code;
!
!	change to temp stack and call pthread_handle_one_process_signal(sig)
!	then jumps into regular scheduler
!	This is called by the universal signal handler to minimize calls
!	to sigsetmask() which is an expensive UNIX system call.
!	assumes SET_KERNEL_FLAG

	.global _pthread_signal_sched
_pthread_signal_sched:
	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window

	ta	ST_FLUSH_WINDOWS	! Flush all windows after init save

	set	_pthread_kern,%l3	! Get kernel address
	ld	[%l3+pthread_self],%l0	! Get current thread

	sethi	%hi(_errno),%l5		! Save the thread context
	ld	[%l5+%lo(_errno)],%g2	! Get errno
	st	%g2,[%l0+thread_errno]	! Save errno of current thread
	std	%fp,[%l0+sp_offset]	! Save the current stack pointer
!impl.	st	%i7,[%l0+pc_offset]	! Save the current return pc

					! Load temp. stack address
	sethi	%hi(_pthread_tempstack-SA(WINDOWSIZE)),%g2	
	mov	%i0, %o0		! Parameter 1: signal number
	mov	%i1, %o1		! Parameter 2: signal code
	call	_pthread_handle_one_process_signal
					! Delay: switch to _tempstack
	add	%g2,%lo(_pthread_tempstack-SA(WINDOWSIZE)),%sp

	ba	restore_stack
	ld	[%l3+ready_head],%o0	! Delay: parameter ready.head

#endif C_CONTEXT_SWITCH

!	int setjmp(env)
!	struct jmp_buf *env;
!
!	setjmp - set up jump environment across procedures, preserve signal mask
!	longjmp() will return to this place and supply a return value
	.global _setjmp
_setjmp:
	sethi	%hi(_errno),%o2		! Get errno address
	ld	[%o2+%lo(_errno)],%o2	! Copy errno
	st	%o2,[%o0+jmp_errno]	!   into environemnt
	st	%sp,[%o0+jmp_sp]	! Save stack pointer
	st	%o7,[%o0+jmp_pc]	! Save return PC
	set	_pthread_kern,%o2	! Get kernel address
	ld	[%o2+pthread_self],%o2	! Get current thread
	ld	[%o2+mask],%o2		! Copy thread mask
	st	%o2,[%o0+jmp_mask]	!   into environment
	retl				! leaf return
	mov	0, %o0			! return 0

!	void longjmp(env, val)
!	struct jmp_buf *env;
!	int val;
!
!	longjmp - execute jump across procedures according to save environment
!	and return specified value at corresponding setjmp().
	.global _longjmp
_longjmp:
	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window
	add	%g0,SIG_SETMASK,%o0	! change_signal_mask(SIG_SETMASK,
	ld	[%i0+jmp_mask],%o1	!                    saved_mask,
	call	_pthread_change_signal_mask
	mov	0, %o2			! Delay:             NULL);
	ld	[%i0+jmp_errno],%o1	! Get thread errno
	sethi	%hi(_errno),%o2		! Get errno address
	st	%o1,[%o2+%lo(_errno)]	! Copy thread errno into global errno
	ta	ST_FLUSH_WINDOWS	! Flush windows to cause underlow
					! on restore
	ld	[%i0+sigjmp_sp],%fp	! Restore stack pointer
	orcc	%i1,%g0,%o0		! val == 0 ?
	bne	longjmp_ret
	ld	[%i0+jmp_pc],%i7	! Delay: Restore return PC
	add	%g0,1,%o0		! val = 1
longjmp_ret:
	ret				! return(val);
	restore	%o0, 0, %o0

!	int sigsetjmp(env, savemask)
!	struct jmp_buf *env;
!	int savemask;
!
!	sigsetjmp - set up jump environment across procedures,
!	preserve signal mask if savemask non-zero
!	siglongjmp() will return to this place and supply a return value
	.global _sigsetjmp
_sigsetjmp:
	sethi	%hi(_errno),%o2		! Get errno address
	ld	[%o2+%lo(_errno)],%o2	! Copy errno
	st	%o2,[%o0+sigjmp_errno]	!   into environemnt
	st	%sp,[%o0+sigjmp_sp]	! Save stack pointer
	tst	%o1			! savemask == 0 ?
	be	sigsetjmp_nomask
	st	%o7,[%o0+sigjmp_pc]	! Delay: Save return PC
	set	_pthread_kern,%o2	! Get kernel address
	ld	[%o2+pthread_self],%o2	! Get current thread
	ld	[%o2+mask],%o2		! Copy thread mask
sigsetjmp_savemask:
	st	%o2,[%o0+sigjmp_mask]	!   into environment
	retl				! leaf return
	mov	0, %o0			! return 0
sigsetjmp_nomask:
	b	sigsetjmp_savemask
	add	%g0,-1,%o2		! set mask to -1 in environment

!	void siglongjmp(env, val)
!	struct jmp_buf *env;
!	int val;
!
!	siglongjmp - exec. jump across procedures according to save environment
!	and return specified value at corresponding sigsetjmp().
	.global _siglongjmp
_siglongjmp:
	save	%sp,-SA(WINDOWSIZE),%sp	! Get a new window
	ld	[%i0+sigjmp_mask],%o1	! Paramter 1: saved_mask
	subcc	%o1,-1,%g0		! saved_mask == -1 ?
	be	siglongjmp_nomask
	add	%g0,SIG_SETMASK,%o0	! Delay: Parameter 0: SIG_SETMASK
	call	_pthread_change_signal_mask
	mov	0, %o2			! Delay: Parameter 2: NULL
siglongjmp_nomask:
	ld	[%i0+sigjmp_errno],%o1	! Get thread errno
	sethi	%hi(_errno),%o2		! Get errno address
	st	%o1,[%o2+%lo(_errno)]	! Copy thread errno into global errno
	ta	ST_FLUSH_WINDOWS	! Flush windows to cause underlow
					! on return
	ld	[%i0+sigjmp_sp],%fp	! Restore stack pointer
	orcc	%i1,%g0,%o0		! val == 0 ?
	bne	siglongjmp_ret
	ld	[%i0+sigjmp_pc],%i7	! Delay: Restore return PC
	add	%g0,1,%o0		! val = 1
siglongjmp_ret:
	ret				! return(val);
	restore	%o0, 0, %o0

#ifdef NOERR_CHECK
!	int pthread_mutex_lock(mutex)
!	pthread_mutex_t *mutex;
!
!	When mutex can be locked immediately, execute as leaf;
!	otherwise call C routine to suspend.
	.global _pthread_mutex_lock
_pthread_mutex_lock:
#ifdef _POSIX_THREADS_PRIO_PROTECT
                                        ! load protocol type of mutex
	ld      [%o0+mutex_protocol],%o1
	cmp     %o1, PRIO_PROTECT	! test if PRIO_PROTECT
	be      slow_lock               ! yes
	nop                             ! Delay
#endif
	ldstub	[%o0+mutex_lock],%o1	! test_and_set(&mutex->lock)
	tst	%o1			! prev. lock == 0 ?
	bne	mutex_locked
	sethi	%hi(_pthread_kern),%o1	! Get kernel address	
	or	%o1,%lo(_pthread_kern),%o1
	ld	[%o1+pthread_self],%o1
	st	%o1,[%o0+mutex_owner]	! mutex->owner = mac_pthread_self()
	retl
	mov	0, %o0			! return(0);
#ifdef _POSIX_THREADS_PRIO_PROTECT
slow_lock:
#endif
mutex_locked:
	save	%sp,-SA(MINFRAME),%sp	! Get a new window
	call	_slow_mutex_lock	! return(slow_mutex_lock(mutex));
	mov	%i0, %o0		! Delay: Pass parameter
	mov	%o0, %i0		! Return value
	ret
	restore

!	int pthread_mutex_trylock(mutex)
!	pthread_mutex_t *mutex;
!
!	When mutex can be locked immediately, execute as leaf and return 1;
!	otherwise return 0
	.global _pthread_mutex_trylock
_pthread_mutex_trylock:
#ifdef _POSIX_THREADS_PRIO_PROTECT
                                        ! load protocol type of mutex
	ld      [%o0+mutex_protocol],%o1
	cmp     %o1, PRIO_PROTECT	! test if PRIO_PROTECT
	bne     fast_trylock		! yes
	nop                             ! Delay
	save	%sp,-SA(MINFRAME),%sp	! Get a new window
	call	_slow_mutex_trylock	! return(slow_mutex_lock(mutex));
	mov	%i0, %o0		! Delay: Pass parameter
	mov	%o0, %i0		! Return value
	ret
	restore
fast_trylock:
#endif
	ldstub	[%o0+mutex_lock],%o1	! test_and_set(&mutex->lock)
	tst	%o1			! prev. lock == 0 ?
	bne	mutex_trylocked
	sethi	%hi(_pthread_kern),%o1	! Get kernel address	
	or	%o1,%lo(_pthread_kern),%o1
	ld	[%o1+pthread_self],%o1
	st	%o1,[%o0+mutex_owner]	! mutex->owner = mac_pthread_self()
	retl
	mov	0, %o0			! return(0);
mutex_trylocked:
	set	_errno,%o1		! errno = EBUSY;
	mov	EBUSY,%o0
	st	%o0,[%o1]
	retl
	mov	-1,%o0		! return(-1);

!	int pthread_mutex_unlock(mutex)
!	pthread_mutex_t *mutex;
!
!	When mutex can be unlocked immediately, execute as leaf;
!	otherwise call C routine to wakeup a thread blocked on the mutex
	.global _pthread_mutex_unlock
_pthread_mutex_unlock:
#ifdef _POSIX_THREADS_PRIO_PROTECT
                                        ! load protocol type of mutex
	ld      [%o0+mutex_protocol],%o1
	cmp     %o1, PRIO_PROTECT	! test if PRIO_PROTECT
	be      slow_unlock             ! yes
	nop                             ! Delay
#endif
	ld	[%o0+mutex_queue],%o1
	tst	%o1			! mutex->queue.head == NULL ?
	bne	mutex_unlocked
	st	%g0,[%o0+mutex_owner]	! Delay: mutex->owner = NO_PTHREAD;
	st	%g0,[%o0+mutex_lock]	! mutex->lock = FALSE;
	ld	[%o0+mutex_queue],%o1
	tst	%o1			! mutex->queue.head == NULL ?
	bne	queue_unlocked
	nop
ret_unlocked:
	retl
	mov	0, %o0			! return(0);
queue_unlocked:
	ldstub	[%o0+mutex_lock],%o1	! test_and_set(&mutex->lock)
	tst	%o1			! prev. lock == 0 ?
	bne	ret_unlocked
	nop
#ifdef _POSIX_THREADS_PRIO_PROTECT
slow_unlock:
#endif
mutex_unlocked:
	save	%sp,-SA(MINFRAME),%sp	! Get a new window
	call	_slow_mutex_unlock	! return(slow_mutex_unlock(mutex));
	mov	%i0, %o0		! Pass parameter
	mov	%o0, %i0		! Return value
	ret
	restore
#endif

#ifndef CLEANUP_HEAP
!	int pthread_cleanup_push(func, arg)
!	void (*func)();
!	any_t arg;
!
!	Get space on stack for cleanup structure (new) and then call
!	pthread_cleanup_push_body with this "new"
	.global _pthread_cleanup_push
_pthread_cleanup_push:
					! Get space for cleanup 	
#ifndef C_INTERFACE
	add	%sp,-SA(cleanup_size+SA(MINFRAME)-WINDOWSIZE),%sp
#else	
	add	%sp,-SA(cleanup_size),%sp
#endif
	b	_pthread_cleanup_push_body
	add	%sp,SA(MINFRAME),%o2	! Delay: param 3 "new"
! 	Should never reach here

!	int pthread_cleanup_pop(execute)
!	int execute;
!
!	Remove space from stack for cleanup structure (new) and 
!	then call pthread_cleanup_pop_body.
	.global	_pthread_cleanup_pop
_pthread_cleanup_pop:
	save	%sp,-SA(MINFRAME),%sp	! Get a new frame
	call	_pthread_cleanup_pop_body
	mov	%i0, %o0		! Delay: param 1
					! Remove space for cleanup 	
	restore				! Restore here, otherwise %sp
					!   increment does not work.
	retl				! Leaf return - already restored.
#ifndef C_INTERFACE
					! Delay: restore prev. stack pointer
	add	%sp,SA(cleanup_size+SA(MINFRAME)-WINDOWSIZE),%sp
#else	
	add	%sp,SA(cleanup_size),%sp
#endif C_INTERFACE
#endif CLEANUP_HEAP

!#ifdef C_INTERFACE  always included, not just for C
!	void start_float()
!
!	start_float is an entry called by crt*.o right before _main.
!	Since start_float is not used as of now, we convert it into
!	a hook to initialize the Pthreads library. Consequently, this
!	definition should overwrite the entry in the C library.
!	CAUTION: (a) If any later version of the SunOS modifies start_float,
!		     this might not work anymore!
!		 (b) This might not work for architectures other than SPARC!
!		     The m68k Sun3, for example, uses this call to initialize
!		     the floating point coprocessor.
!		 (c) Languages other than C have to provide a similar hook
!		     inside the start entry (see ld(1)) or the user has
!		     call pthread_init() explicitly before using any
!		     Pthreads routines.
	.global start_float
start_float:
	save	%sp,-SA(MINFRAME),%sp
	call	_pthread_init		! Initialize pthreads
	nop				! Delay:
	ret
	restore
!#endif              always included, not just for C
