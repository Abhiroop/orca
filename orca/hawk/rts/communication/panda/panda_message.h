#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "pan_sys.h"
#include "message.h"

#define TYPE_MSG  1
#define TYPE_JOIN 2
typedef unsigned short msg_type_t;

/* The message data contains three parts:
 * - RTS communication header
 * - RTS user header
 * - user data
 *
 * The RTS communication header, msg_hdr, contains information about the
 * size of the user header and the user data. Furthermore, it contains
 * the header fields for the mp_channel and grp_channel modules.
 *
 * XXX: currently, alignment is not handled correctly. The communication
 * header looks to be OK. The rest of the RTS is responsible for
 * providing correctly aligned user headers that do not cause the data
 * part to be disaligned.
 */

typedef struct msg_hdr {
    int            data_size;
    unsigned short hdr_size;
    unsigned short msg_handler;
    unsigned short ch_id;
    unsigned short sender;
    unsigned short sync_id;
    msg_type_t     type;
}msg_hdr_t, *msg_hdr_p;



/* Layout:
 *
 *          ------------------------------------------------------
 * data ->  | comm hdr | user hdr | data  | free space | trailer |
 *          ------------------------------------------------------
 *          ^          \_________/ \___________________/
 *          |               |      \______/  |
 *          |               |          |    size
 * hdr -----                |          |
 * - hdr_size ---------------          |
 * - data_size ------------------------
 *
 *
 */

struct message_s {
    char     *data;		/* Pointer to data buffer */
    int       size;		/* size of user data part */
    msg_hdr_p hdr;		/* Pointer to message header */
};


message_p rts_message_receive(void *data, int size, int len);
void      rts_message_trailer(int t);
int       rts_message_length(message_p m);

#endif /* __MESSAGE_H__ */
