/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Id: macro.h,v 1.1 1999/10/11 14:23:41 ceriel Exp $ */
/* PREPROCESSOR: DEFINITION OF MACRO DESCRIPTOR */

/*	The flags of the mc_flag field of the macro structure. Note that
	these flags can be set simultaneously.
*/
#define NOFLAG		0		/* no special flags	*/
#define	FUNC		0x1		/* function attached    */
#define NOUNDEF		0x2		/* reserved macro	*/
#define NOREPLACE	0x4		/* prevent recursion	*/

#define	FORMALP 0200	/* mask for creating macro formal parameter	*/

/*	The macro descriptor is very simple, except the fact that the
	mc_text, which points to the replacement text, contains the
	non-ascii characters \201, \202, etc, indicating the position of a
	formal parameter in this text.
*/
struct macro	{
	char *	mc_text;	/* the replacement text		*/
	int	mc_nps;		/* number of formal parameters	*/
	int	mc_length;	/* length of replacement text	*/
	char	mc_flag;	/* marking this macro		*/
};


/* allocation definitions of struct macro */
extern char *st_alloc();
extern struct macro *h_macro;
#define	new_macro() ((struct macro *) st_alloc((char **)&h_macro, sizeof(struct macro), 20))
#define	free_macro(p) st_free(p, &h_macro, sizeof(struct macro))


/* `token' numbers of keywords of command-line processor
*/
#define	K_UNKNOWN	0
#define	K_DEFINE	1
#define	K_ELIF		2
#define	K_ELSE		3
#define	K_ENDIF		4
#define	K_ERROR		5
#define	K_IF		6
#define	K_IFDEF		7
#define	K_IFNDEF	8
#define	K_INCLUDE	9
#define	K_LINE		10
#define	K_PRAGMA	11
#define	K_UNDEF		12
#define K_FILE		100	/* for dependency generator */
