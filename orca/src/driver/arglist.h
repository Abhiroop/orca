/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: arglist.h,v 1.5 1997/04/02 09:10:24 ceriel Exp $ */

/*   A R G U M E N T   L I S T   H A N D L I N G   */

struct arglist {
	int	argc;
	int	maxargc;
	char 	**args;
};

_PROTOTYPE(void append, (char *arg, struct arglist *ap));
/* Append the string "arg" to the argument list "ap".
*/

_PROTOTYPE(int append_unique, (char *arg, struct arglist *ap));
/* Append the string "arg" to the argument list "ap", but only if
   "ap" does not contain it yet. Return 1 if it is appended, 0 if it is not.
*/

_PROTOTYPE(void split_and_append, (char *args, struct arglist *ap));
/* Split the "args" space-separated string into pieces and add the pieces
   to the arglist indicated by "ap".
*/

_PROTOTYPE(void pr_vec, (struct arglist *vec, FILE *f, int termch));
/* Print the argument list 'vec' onto file descriptor f, and terminate with
   a 'termch'.
*/
