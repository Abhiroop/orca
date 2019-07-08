#ifndef _SYS_ACTMSG_CONST_
#define _SYS_ACTMSG_CONST_

#define 	STACK_SIZE	102400
#define		MAX_THREAD	8
#define		BODY_OFFSET	200
#define 	RETURN_OFFSET	8
#define         MAX_KEY         16

/* Possible states of a thread; */
#define		T_FREE		0x1
#define		T_RUNNING	0x2
#define		T_RUNNABLE	0x4
#define		T_DETACHED	0x8
#define		T_JOINED	0x10

#ifdef AMOEBA

#include "offset.h"
#include "fault.h"

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN	8
#define	SA(X)	(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))

#endif /* AMOEBA */

#endif /* _SYS_ACTMSG_CONST_ */
