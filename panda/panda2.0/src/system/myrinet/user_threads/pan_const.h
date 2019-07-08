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

#ifdef AMOEBA
/*
 * A stack frame looks like:
 *
 * %fp->|				|
 *	|-------------------------------|
 *	|  Locals, temps, saved floats	|
 *	|-------------------------------|
 *	|  outgoing parameters past 6	|
 *	|-------------------------------|-\
 *	|  6 words for callee to dump	| |
 *	|  register arguments		| |
 *	|-------------------------------|  > minimum stack frame
 *	|  One word struct-ret address	| |
 *	|-------------------------------| |
 *	|  16 words to save IN and	| |
 * %sp->|  LOCAL register on overflow	| |
 *	|-------------------------------|-/
 */

/*
 * Constants defining a stack frame.
 */
#define	WINDOWSIZE	(16*4)		/* size of window save area */
#define	ARGPUSHSIZE	(6*4)		/* size of arg dump area */
#define	ARGPUSH		(WINDOWSIZE+4)	/* arg dump area offset */
#define	MINFRAME	(WINDOWSIZE+ARGPUSHSIZE+4) /* min frame */

/*
 * Stack alignment macros.
 */
#define	STACK_ALIGN	8
#define	SA(X)	(((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))

#endif /* AMOEBA */

#endif /* _SYS_ACTMSG_CONST_ */
