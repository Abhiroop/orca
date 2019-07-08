#ifndef __grp_channel__
#define __grp_channel__

/*********************************************************************/
/*********************************************************************/
/* Group communication. */
/*********************************************************************/
/*********************************************************************/

#include "message.h"
#include "set.h"

typedef struct grp_channel_s *grp_channel_p, grp_channel_t;

int init_grp_channel(int moi, int gsize, int pdebug);
int finish_grp_channel(void);

grp_channel_p new_grp_channel(set_p processors);
int free_grp_channel(grp_channel_p gch);

int grp_channel_send(grp_channel_p gch, message_p);

#endif
