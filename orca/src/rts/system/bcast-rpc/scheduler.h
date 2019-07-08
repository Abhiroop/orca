/* $Id: scheduler.h,v 1.4 1994/03/11 10:52:12 ceriel Exp $ */

/* Process structures are created dynamically (by the run time system),
 * one for each process.
 */

typedef struct process * process_p;

/* Amoeba Implementation process */
struct process {
	prc_dscr *descr;
	void	**args;
	int	process_id;
	int	DoingOperation;		/* busy in operation? */
	int		ResultOperation; /* tmp to store the result of a op */
	char 		*buf; 		/* buf to marshall */
	semaphore 	SendDone; 	/* sync variable for send/rcv */
	int		received;	/* received the msg just sent? */
	semaphore 	suspend; 	/* sync variable for rescheduling */
	process_p	next;		/* list of processes */
	int		blocking;
};

#define GET_TASKDATA() (process_p) thread_param(0)

#ifdef sparc
#define STACKSIZE	16000		/* Recommended by Greg for Sparcs */
#else
#define STACKSIZE	5000
#endif
