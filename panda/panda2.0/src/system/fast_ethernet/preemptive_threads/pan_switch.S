#ifdef CMOST

#include <sys/errno.h>
#include <sun4/trap.h>
#define KERNEL
#include <sun4/asm_linkage.h>
#undef KERNEL

#elif AMOEBA

#define _ASM
#include <syscall.h>
#include <syscall_stub.h>
#include "pan_const.h"

#endif

#define	t_sp	0

	.seg "text"

!	void pan_thread_switch( pan_thread_p cur, pan_thread_p next)
!

	.globl _pan_thread_switch
_pan_thread_switch:
	save 	%sp, -SA(MINFRAME),%sp		! Get new window

#ifdef CMOST
!	There used to be a bug in the register window code
!	of CMOST; this is the workaround.

	save    %sp, -SA(MINFRAME), %sp
        save    %sp, -SA(MINFRAME), %sp
        save    %sp, -SA(MINFRAME), %sp
        save    %sp, -SA(MINFRAME), %sp
        save    %sp, -SA(MINFRAME), %sp
        save    %sp, -SA(MINFRAME), %sp
        restore
        restore
        restore
        restore
        restore
        restore
#elif AMOEBA
!	Under Amoeba, we use sys_null() to flush the register
!	windows (I hope). Unfortunately, sys_null() uses g1 and g2,
!	so we have to reload these when sys_null() returns. Sorry.

	_SCALL(sys_null)
#else
	! ta	ST_FLUSH_WINDOWS		! Flush windows current thread
#endif
	std	%fp,[%i0+t_sp]			! Save the current sp
	ldd	[%i1+t_sp],%fp			! Load new sp

! Return into the new context.  Since all windows have been flushed, the
! RESTORE will cause a window underflow trap, restoring the registers of the
! new context from its stack.
	restore
	retl
	nop



!	void pan_thread_switch_fast( void (* fun)(void),
!				     long int stack, pan_thread_p blocked)
!
	.align 4
	.globl _pan_thread_switch_fast
_pan_thread_switch_fast:
	save	%o1, -SA(MINFRAME), %sp		! Switch to new stack AND
						! advance reg window so current
						! threads regs will be saved
						! at old stack.
	call	%i0				! Execute new thread.
	std	%fp, [%i2 + t_sp]		! Save sp/pc for a stack split
	ret
	restore					! automatically restores old sp


	.align 4
	.global _pan_asm_get_sp
_pan_asm_get_sp:
	mov %sp, %o0
	retl
	nop

	.align 4
	.global _pan_asm_set_sp
_pan_asm_set_sp:
	mov %o0, %sp
	retl
	nop

	.align 4
	.global _pan_asm_get_fp
_pan_asm_get_fp:
	mov %fp, %o0
	retl
	nop

	.align 4
	.global _pan_asm_set_fp
_pan_asm_set_fp:
	mov %o0, %fp
	retl
	nop

	.align 4
	.global ___trans
___trans:
	_SCALL(trans)
	retl
	nop
