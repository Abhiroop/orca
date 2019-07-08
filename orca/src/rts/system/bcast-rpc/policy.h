/* $Id: policy.h,v 1.4 1994/03/21 11:54:15 ceriel Exp $ */

#define NPOLICY			3
#define BASE_NUMBER		256
#define THRESHHOLD		5
#define COST_BROADCAST(n)	(2700+n*7)	/* usec */
#define COST_RPC		2500		/* usec */

#define MASTER_ALL_ON_ZERO		1	/* masters on cpu 0 */
#define REPLICATE_ALL			2	/* replicate all objects */
#define REPLICATE_ALL_OR_MASTER		3	/* replicate all or 1 master */

#define MAX_RPC_THREADS		65		/* max # of rpc_listeners */

