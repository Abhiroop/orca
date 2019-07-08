/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* A package for lists of group members.
 * Implemented as descriptor +
 *		array of struct { seqno; messid }
 * Used by the sequencer to maintain its member admin and used by members
 * to identify bb data retransmits.
 */


#ifndef _GROUP_MEMLIST_H
#define _GROUP_MEMLIST_H


#include "pan_sys.h"

#include "grp_types.h"


typedef struct MEM_INFO_T {
    int			seqno;		/* member's next-to-be received frag
					 * If pre-start or post-leave, equals
					 * INT_MAX */
    unsigned short int	messid;		/* member's next-to-be req's frag */
}   mem_info_t, *mem_info_p;


typedef struct MEMBER_LST_T {
    int			n_members;	/* Size of info array */
    int          	upb_member;	/* Upper bound of member no in use */
    mem_info_p		info;		/* member info array */
}   mem_list_t, *mem_list_p;


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif




#ifndef STATIC_CI

void pan_mem_lst_messid_set(mem_list_p lst, int mem, unsigned short int id);
unsigned short int pan_mem_lst_messid_get(mem_list_p lst, int mem);

void pan_mem_lst_seqno_set(mem_list_p lst, int mem, int sqno);
int  pan_mem_lst_seqno_get(mem_list_p lst, int mem);

int  pan_mem_lst_seqno_min(mem_list_p lst);

int  pan_mem_lst_n_behind(mem_list_p lst, int min_sq);

void pan_mem_add_member(mem_list_p lst, int me, int seqno);
int  pan_mem_del_member(mem_list_p lst, int me);

void pan_mem_lst_init(mem_list_p lst, int n_members);
void pan_mem_lst_clear(mem_list_p lst);

#endif

#endif
