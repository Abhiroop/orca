#ifndef __PAN_RPC_UPCALL__
#define __PAN_RPC_UPCALL__

#include "pan_rpc.h"


				/* Here comes the dirty trick: encode the
				 * client id in the pointer of the pan_upcall_p,
				 * not in the struct it points to. This saves
				 * a malloc/free (on the critical path).
				 */
typedef union CLIENT_ID_T {
    struct {
	short int    pid;
	short int    ticket;
    }			uus;
    pan_upcall_p	ptr;
} client_id_t, *client_id_p;


typedef enum PAN_RPC_TYPE_T {
    RPC_REQUEST,
    RPC_REPLY
} pan_rpc_type_t, *pan_rpc_type_p;


typedef struct RPC_HDR_T {
    client_id_t		client;
    unsigned char	type;	/* should be a pan_rpc_type_t, but compress */
} rpc_hdr_t, *rpc_hdr_p;


typedef struct pan_upcall{
    int			dummy;
}pan_upcall_t;



#ifndef STATIC_CI
#  define STATIC_CI	static
#endif

STATIC_CI void		pan_rpc_upcall_start(void);
STATIC_CI void		pan_rpc_upcall_end(void);


STATIC_CI rpc_hdr_p	pan_rpc_hdr_push(pan_msg_p msg);

STATIC_CI rpc_hdr_p	pan_rpc_hdr_pop(pan_msg_p msg);


#endif /* __PAN_RPC_UPCALL__ */
