#ifndef __mp_channel__
#define __mp_channel__

/*********************************************************************/
/*********************************************************************/
/* Unreliable message passing interface. */
/*********************************************************************/
/*********************************************************************/

#include "message.h"
#include "set.h"

typedef struct mp_channel_s mp_channel_t, *mp_channel_p;

int init_mp_channel(int moi, int gsize, int pdbug);
int finish_mp_channel(void);

mp_channel_p new_mp_channel(set_p group);
int free_mp_channel(mp_channel_p);

int mp_channel_unicast(mp_channel_p, int receiver, message_p);
int mp_channel_multicast(mp_channel_p, message_p);
int print_ch_number(mp_channel_p ch);

#endif
