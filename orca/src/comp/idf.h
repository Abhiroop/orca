/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __IDF_H__
#define __IDF_H__

/* U S E R   D E C L A R E D   P A R T	 O F   I D F */

/* $Id: idf.h,v 1.9 1997/07/02 14:12:06 ceriel Exp $ */

struct id_u {			/* Structure associated with each identifier. */
	int	id_res;		/* If it is a reserved word, its token value. */
	int	id_macro;
	int	id_ismacro;
	struct def
		*id_df;		/* A list of possible identifications. */
};

#define IDF_TYPE	struct id_u
#define id_reserved	id_user.id_res
#define id_def		id_user.id_df
#define id_resmac	id_user.id_macro
#define id_mac		id_user.id_ismacro

#include	<idf_pkg.spec>

typedef struct idf	t_idf, *p_idf;

#include "list.h"

/* Mechanism for building lists of identifiers. */

typedef p_list t_idlst;
#define idf_initlist(pq)	initlist(pq)
#define idf_enlist(pq, e)	enlist(pq, (void *) (e))
#define idf_endlist(pq)		endlist(pq)
#define idf_killlist(pq)	killlist(pq)
#define idf_emptylist(q)	emptylist(q)
#define idf_walklist(p,l,i)	walklist(p, l, i, p_idf)

#define idf_dummylist(pq)	do { /* Create a list containing one dummy \
					entry. \
				     */ \
				     idf_initlist(pq); \
				     idf_enlist(pq, (p_idf) 0); \
				     idf_endlist(pq); \
				} while (0)
#endif /* __IDF_H__ */
