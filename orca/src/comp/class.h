/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __CLASS_H__
#define __CLASS_H__

/* U S E   O F	 C H A R A C T E R   C L A S S E S */

/* $Id: class.h,v 1.7 1997/05/15 12:01:41 ceriel Exp $ */

/* As a starter, chars are divided into classes, according to which
   token they can be the start of.
*/

#define	class(ch)	(tkclass[ch])

/* Being the start of a token is, fortunately, a mutual exclusive
   property.
*/

#define	STSKIP	0	/* spaces and so on: skipped characters		*/
#define	STNL	1	/* newline character(s): update linenumber etc.	*/
#define	STGARB	2	/* garbage ascii character: not allowed		*/
#define	STSIMP	3	/* this character can occur as token		*/
#define	STCOMP	4	/* this one can start a compound token		*/
#define	STIDF	5	/* being the initial character of an identifier	*/
#define	STCHAR	6	/* the starter of a character constant		*/
#define	STSTR	7	/* the starter of a string			*/
#define	STNUM	8	/* the starter of a numeric constant		*/
#define	STEOI	9	/* End-Of-Information mark			*/

/* But occurring inside a token is not, so we need 1 bit for each
   class.  This is implemented as a collection of tables to speed up
   the decision whether a character has a special meaning.
*/
#define	in_idf(ch)	((unsigned)ch < 0177 && inidf[(int)(ch)])
#define	is_dig(ch)	((unsigned)ch < 0177 && isdig[(int)(ch)])
#define	is_oct(ch)	((unsigned)ch < 0177 && isoct[(int)(ch)])
#define	is_hex(ch)	((unsigned)ch < 0177 && ishex[(int)(ch)])

extern char tkclass[];
extern char inidf[], isdig[], isoct[], ishex[];
#endif /* __CLASS_H__ */
