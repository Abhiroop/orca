/* $Header: /usr/proj/panda/Repositories/gnucvs/panda2.0/src/system/cmaml/malloc/log.h,v 1.1 1996/03/08 10:59:26 tim Exp $ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	Algorithms to manipulate the doubly-linked lists of free
	chunks.
*/

public void link_free_chunk(mallink *ml);
public void unlink_free_chunk(mallink *ml);
public mallink *first_present(int class);
public mallink *search_free_list(int class, unsigned int n);

#ifdef STORE
#define in_store(ml)		((size_type)_phys_prev_of(ml) & STORE_BIT)
#define set_store(ml, e) \
	(_phys_prev_of(ml) = (mallink *) \
		((e) ? (size_type) _phys_prev_of(ml) | STORE_BIT : \
		       (size_type) _phys_prev_of(ml) & ~STORE_BIT))
#endif
#define	set_log_prev(ml,e)	(_log_prev_of(ml) = (e))
#define	log_prev_of(ml)		(mallink *) (_log_prev_of(ml))

#define	set_log_next(ml,e)	(_log_next_of(ml) = (e))
#define	log_next_of(ml)		(mallink *) (_log_next_of(ml))

