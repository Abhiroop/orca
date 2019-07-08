#ifndef __PAN_SYS_SEQ_H__
#define __PAN_SYS_SEQ_H__


			/* Start bcast sequencer module */
void pan_comm_bcast_seq_start(int n_servers);

			/* End bcast sequencer module */
void pan_comm_bcast_seq_end(void);

			/* Fill in info slots of sequencer module */
void pan_comm_bcast_seq_info(void);

#endif
