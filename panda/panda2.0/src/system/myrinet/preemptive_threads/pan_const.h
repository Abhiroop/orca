#ifndef _SYS_ACTMSG_CONST_
#define _SYS_ACTMSG_CONST_

#define 	STACK_SIZE	102400
#define		MAX_THREAD	16
#define		BODY_OFFSET	200
#define 	RETURN_OFFSET	8
#define         MAX_KEY         16

/* Possible states of a thread; */
#define		T_FREE		0x1
#define		T_RUNNING	0x2
#define		T_RUNNABLE	0x4
#define		T_DETACHED	0x8
#define		T_JOINED	0x10
#define		T_SIGNAL_SELF	0x20
#define		T_BLOCKED	0x40
#define		T_TIMEOUT	0x80
#define		T_HANDLER	0x100

#ifdef AMOEBA

#include "offset.h"
#include "fault.h"	/* includes definition of MINFRAME */

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN	8
#define	SA(X)	(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))

#endif /* AMOEBA */

#endif /* _SYS_ACTMSG_CONST_ */
