#ifndef __PAN_SYS_SND_BCAST_H__
#define __PAN_SYS_SND_BCAST_H__


#include "pan_sys.h"

#include "pan_msg_cntr.h"


/* Constants for mailbox msg exchange ("random" communication) */

#define REPLY_CODE 2		/* Code: on reply from sequencer to sender */
#define REQ_TYPE   1		/* Message type:
				 * message type for both directions */


			/* Use this to broadcast meta messages if you are the
			 * sequencer, and already know seqno and control flags
			 */
void pan_comm_bcast_snd_small(void *data, int control, int seqno, int sender,
			      pan_msg_counter_p x_counter);

			/* Send control msg to the sequencer */
void pan_comm_send_control_msg(sys_msg_tag_t event);

			/* Fill in bcast send info */
void pan_comm_bcast_snd_info(void);

			/* Start bcast send module */
void pan_comm_bcast_snd_start(void);

			/* Stop bcast send module */
void pan_comm_bcast_snd_end(void);


#endif
