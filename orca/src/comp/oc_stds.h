/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __STANDARDS_H__
#define __STANDARDS_H__

/* S T A N D A R D   P R O C E D U R E S   A N D   F U N C T I O N S */

/* $Id: oc_stds.h,v 1.2 1997/05/15 12:02:35 ceriel Exp $ */

/* discrete-valued built-ins (value below MAXI): */

#define S_CAP	1
#define S_CHR	2
#define S_MAX	3
#define S_MIN	4
#define S_ODD	5
#define S_ORD	6
#define S_VAL	7
#define S_SIZE	8
#define S_LB	9
#define S_UB	10

#define MAXI	20

/* built-ins operating on reals and integers (value above MAXI, below MAXIF) */
#define S_ABS	(MAXI+1)

#define MAXIF	(MAXI+20)

/* floating point built-ins (value above MAXIF, below MAXF) */
#define S_FLOAT	(MAXIF+1)
#define S_TRUNC	(MAXIF+2)

#define MAXF	(MAXIF+20)

/* miscellaneous */

#define S_NCPUS		(MAXF+1)
#define S_MYCPU		(MAXF+2)
#define S_ASSERT	(MAXF+3)
#define S_WRITE		(MAXF+4)
#define S_WRITELN	(MAXF+5)
#define S_FROM		(MAXF+6)
#define S_ADDNODE	(MAXF+7)
#define S_DELETENODE	(MAXF+8)
#define S_DELETE	(MAXF+9)
#define S_INSERT	(MAXF+10)
#define S_READ		(MAXF+11)
#ifndef NOSTRATEGY
#define S_STRATEGY	(MAXF+20)
#endif
#endif /* __STANDARDS_H__ */
