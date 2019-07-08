/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: replace.h,v 1.1 1999/10/11 14:23:44 ceriel Exp $ */
/* 	DEFINITIONS FOR THE MACRO REPLACEMENT ROUTINES		*/

struct repl {
	struct	repl *next;
	struct	idf *r_idf;		/* name of the macro */
	struct	args *r_args;		/* replacement parameters */
	int	r_level;		/* level of insertion */
	int	r_size;			/* current size of replacement buffer */
	char	*r_ptr;			/* replacement text index pointer */
	char	*r_text;		/* replacement text */
};


/* allocation definitions of struct repl */
extern char *st_alloc();
extern struct repl *h_repl;
#define	new_repl() ((struct repl *) st_alloc((char **)&h_repl, sizeof(struct repl), 4))
#define	free_repl(p) st_free(p, &h_repl, sizeof(struct repl))


#define NO_REPL		(struct repl *)0

/*	The implementation of the ## operator is currently very clumsy.
	When the the ## operator is used the arguments are taken from
	the raw buffer; this buffer contains a precise copy of the
	original argument. The fully expanded copy is in the arg buffer.
	The two copies are here explicitely because:

		#define ABC	f()
		#define	ABCD	2
		#define	g(x, y)	x ## y + h(x)

		g(ABC, D);

	In this case we need two copies: one raw copy for the pasting
	operator, and an expanded one as argument for h().
*/
struct args {
	char	*a_expptr;		/* expanded argument index pointer */
	char	*a_expbuf;		/* expanded argument buffer pointer */
	int	a_expsize;		/* current size of expanded buffer */
	char	*a_expvec[NPARAMS];	/* expanded argument vector */
	char	*a_rawptr;		/* raw argument index pointer */
	char	*a_rawbuf;		/* raw argument buffer pointer */
	int	a_rawsize;		/* current size of raw buffer */
	char	*a_rawvec[NPARAMS];	/* raw argument vector */
};


/* allocation definitions of struct args */
extern char *st_alloc();
extern struct args *h_args;
#define	new_args() ((struct args *) st_alloc((char **)&h_args, sizeof(struct args), 2))
#define	free_args(p) st_free(p, &h_args, sizeof(struct args))


#define NO_ARGS		(struct args *)0
