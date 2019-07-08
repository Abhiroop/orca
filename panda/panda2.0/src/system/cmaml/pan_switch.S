#ifdef CMOST
#include <sys/errno.h>
#include <sun4/trap.h>
#define KERNEL
#include <sun4/asm_linkage.h>
#undef KERNEL
#endif

#define _ASM
#include "pan_const.h"

#define	t_sp	0

	.seg "text"

!	void pan_thread_switch
!

	.globl _pan_thread_switch
_pan_thread_switch:
	save 	%sp, -SA(MINFRAME),%sp		! Get new window
	set	_pan_cur_thread,%l2		! Get current thread
	set	_pan_next_thread,%l3		! Get next thread
	ld	[%l2],%l0			! Load current thread
	ld	[%l3],%l1			! Load next thread

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

	call	__sys_null
	nop
#else
	! ta	ST_FLUSH_WINDOWS		! Flush windows current thread
#endif
	std	%fp,[%l0+t_sp]			! Save the current sp
	ldd	[%l1+t_sp],%fp			! Load new sp
	st	%l1,[%l2]			! Set current thread
	st	%g0,[%l3]			! Clear next thread

! Return into the new context.  Since all windows have been flushed, the
! RESTORE will cause a window underflow trap, restoring the registers of the
! new context from its stack.
	restore
	retl
	nop
