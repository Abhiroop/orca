/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* Q U E U E   M E C H A N I S M */

/* $Id: list.H,v 1.12 1997/05/15 12:02:22 ceriel Exp $ */

#ifndef __LIST_H__
#define __LIST_H__

#include	"ansi.h"

typedef struct list {
	struct list
		*l_next;
	void	*l_elem;
} t_list, *p_list;

/* ALLOCDEF "list" 50 */

/* While building the list, the list is circular. To make the list permanent,
   endlist should be called, after which the list is linear.
*/
#define delist(pq, pe) \
	{	p_list dummy = (*(pq))->l_next; \
		*(pe) = dummy->l_elem; \
		if (dummy == *(pq)) *(pq) = 0; \
		else (*(pq))->l_next = dummy->l_next; \
		free_list(dummy); \
	}

#define enlist(pq, e) \
	{	p_list dummy = new_list(); \
		dummy->l_elem = (e); \
		if (*(pq)) { \
			dummy->l_next = (*(pq))->l_next; \
			(*(pq))->l_next = dummy; \
		} \
		else dummy->l_next = dummy; \
		*(pq) = dummy; \
	}

#define endlist(pq) \
	{	p_list dummy = *(pq); \
		if (dummy) { \
			*(pq) = dummy->l_next; \
			dummy->l_next = 0; \
		} \
	}

#define _getlistel(l) \
	((l) ? (l)->l_elem : (void *) 0)
/* The (void *) cast is needed to avoid a bug in the SunOs 4.1.3 C compiler. */

#define getlistel(l, tp) \
	((tp) _getlistel(l))

#define setlistel(l, e) \
	((l)->l_elem = (void *) (e))

#define nextlistel(l) \
	((l) ? (l)->l_next : 0)

#define emptylist(l)	((l) == 0)

#define walklist(q, l, e, tp) \
	for (l = (q); (e = getlistel(l, tp)), l != 0; l = nextlistel(l))

#define initlist(pq)	(*(pq) = 0)

#define killlist(pq) \
	{	p_list dummy; \
		while ((dummy = *(pq))) { \
			*(pq) = dummy->l_next; \
			free_list(dummy); \
		} \
	}

typedef struct dbl_list {
	struct dbl_list
		*dbl_prev,
		*dbl_next;
	void	*dbl_elem;
} t_dbl_list, *p_dbl_list;

/* ALLOCDEF "dbl_list" 50 */

/* The dbl_prev chain stays circular, so that we can easily find
   the end of the chain. Therefore, unlike the singly linked list,
   the doubly linked list does not have to be cicrular.
*/

#define dbl_enlist(pq, e) \
	{	p_dbl_list dummy = new_dbl_list(); \
		dummy->dbl_elem = (e); \
		if (*(pq)) { \
			/* dummy->dbl_next = 0; */ \
			dummy->dbl_prev = (*(pq))->dbl_prev; \
			(*(pq))->dbl_prev->dbl_next = dummy; \
			(*(pq))->dbl_prev = dummy; \
		} \
		else { \
			/* dummy->dbl_next = 0; */ \
			dummy->dbl_prev = dummy; \
			*(pq) = dummy; \
		} \
	}

#define dbl_insert(pq, e) \
	{	p_dbl_list dummy = new_dbl_list(); \
		dummy->dbl_elem = (e); \
		if (*(pq)) { \
			dummy->dbl_prev = (*(pq))->dbl_prev; \
			dummy->dbl_next = *(pq); \
			if ((*(pq))->dbl_prev->dbl_next) { \
				(*(pq))->dbl_prev->dbl_next = dummy; \
			} \
			(*(pq))->dbl_prev = dummy; \
		} \
		else { \
			/* dummy->dbl_next = 0; */ \
			dummy->dbl_prev = dummy; \
			*(pq) = dummy; \
		} \
	}

#define dbl_endlist(pq)

#define _dbl_getlistel(l) \
	((l) ? (l)->dbl_elem : (void *) 0)
/* The (void *) cast is needed to avoid a bug in the SunOs 4.1.3 C compiler. */

#define dbl_getlistel(l, tp) \
	((tp) _dbl_getlistel(l))

#define dbl_setlistel(l, e) \
	((l)->dbl_elem = (void *) (e))

#define dbl_nextlistel(l) \
	((l) ? (l)->dbl_next : 0)

#define dbl_prevlistel(l) \
	((l) ? (l)->dbl_prev : 0)

#define dbl_emptylist(l)	((l) == 0)

#define dbl_walklist(q, l, e, tp) \
	for (l = (q); \
	     (e = dbl_getlistel(l, tp)), l != 0; \
	     l = dbl_nextlistel(l))

#define dbl_reversewalklist(q, l, e, tp) \
	for (l = dbl_prevlistel(q); \
	     (e = dbl_getlistel(l, tp)), l != 0; \
	     l = (l == q ? 0 : dbl_prevlistel(l)))

#define dbl_initlist(pq)	(*(pq) = 0)

#define dbl_killlist(pq) \
	{	p_dbl_list dummy; \
		while ((dummy = *(pq))) { \
			*(pq) = dummy->dbl_next; \
			free_dbl_list(dummy); \
		} \
	}

_PROTOTYPE(int listmember, (p_list l, void *p));
	/*	Test if 'p' is a member of the list 'l'.
	*/

_PROTOTYPE(int dbllistmember, (p_dbl_list l, void *p));
	/*	Test if 'p' is a member of the doubly linked list 'l'.
	*/

#endif /* __LIST_H__ */
