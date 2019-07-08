/* $Id: server.h,v 1.2 1994/03/11 10:52:16 ceriel Exp $ */

#define MEMBERCAP	"MEMBERCAP"
#define GROUPCAP	"GROUPCAP"
#define PRIVATECAP	"PRIVATECAP"

/* request from client to server */
#define CP_CHECKPOINT	10
#define CP_ROLLBACK	11
#define CP_REMOVECP	12
#define CP_FINISH	13
#define CP_FLUSH	14

/* instructions from server to client */
#define CP_CONTINUE	20
#define CP_RESTART	21
