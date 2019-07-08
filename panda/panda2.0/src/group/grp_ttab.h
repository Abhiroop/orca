#ifndef __PAN_GRP_TTAB_H__
#define __PAN_GRP_TTAB_H__


#include "pan_sys.h"

#include "grp_header.h"



typedef struct GRP_TTAB_USR_T ttab_usr_t, *ttab_usr_p;

struct GRP_TTAB_USR_T {
				/* Flags written out as booleans */
    boolean		is_msg_arrived;
    boolean		is_home_msg_discarded;
    boolean		is_msg_locked;
    boolean		is_frag_arrived;
    boolean		is_sync_send;

    pan_cond_p          msg_arrived;
    grp_hdr_t		hdr;
    pan_fragment_p	frag;
    pan_msg_p		msg;
    pan_group_p		group;
    int			attempts;
    int			max_attempts;
    int			ticks;
    ttab_usr_p		next;
    ttab_usr_p		prev;
    ttab_usr_p		frozen_next;
};


#ifndef MAX_TTABLE_SLOTS
#define MAX_TTABLE_SLOTS	256
#endif


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

void		ttab_enqueue(ttab_usr_p t, int ticks);
void		ttab_dequeue(ttab_usr_p t);

ttab_usr_p	ttab_get(void);
void		ttab_free(ttab_usr_p t);

short int	ttab_ticket(ttab_usr_p t);
ttab_usr_p	ttab_entry(short int ticket);

void		ttab_freeze_list(void);
ttab_usr_p	ttab_frozen_first(void);
ttab_usr_p	ttab_frozen_next(ttab_usr_p t);

void		ttab_start(pan_mutex_p lock);
void		ttab_end(void);

#endif



#endif
