/* $Id: cp.c,v 1.7 1996/07/04 08:52:50 ceriel Exp $ */

#include <amoeba.h>
#include "thread.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/stdcmd.h"
#include "module/rnd.h"
#include <semaphore.h>
#include "server.h"
#include "global.h"
#include "interface.h"
#include "scheduler.h"

/* This module is the client part of gax. It implements the following 
** interface: 
**	- cp_init:	initialize module.
**	- cp_docp:	ask server for cp.
**	- cp_rollback:	ask server to perform a rollback.
**	- cp_remove:	ask server to remove cp.
*/

extern int this_cpu;
extern FILE *file;
extern int stacksize;
static port pub_port;
static semaphore cp_done;

/* cp_server hears from gax how we came at this point in time. The 
** possibilities are: 1) the process completed a cp or 2) the process just
** completed a rollback. The process has no way of finding this out. So, gax
** tells it.
*/

void
cp_server()
{
    errstat n;
    header hdr;
    port priv_port;

    uniqport(&priv_port);
    priv2pub(&priv_port, &pub_port);
    if (NULLPORT(&pub_port)) {
	fprintf(file, "\t\tsp_server (cpu %d), pub_port = NULL", this_cpu);
    }
    sema_init(&cp_done, 0);
    
    for(;;) {
	hdr.h_port = priv_port;
	n = getreq(&hdr, NILBUF, 0);
	if (ERR_STATUS(n)) {
	    continue;
	}
	switch(hdr.h_command) {
	case CP_CONTINUE:
	    sema_up(&cp_done);
	    hdr.h_status = STD_OK;
	    putrep(&hdr, NILBUF, 0);
	    cp_continue();
	    break;
	case CP_RESTART:
	    cp_initrestart();
	    hdr.h_status = STD_OK;
	    putrep(&hdr, NILBUF, 0);
	    cp_restart();
	    sema_up(&cp_done);
	    break;
	case CP_FINISH:
	    hdr.h_status = STD_OK;
	    putrep(&hdr, NILBUF, 0);
	    thread_exit();
	default:
	    hdr.h_status = STD_COMBAD;
	    putrep(&hdr, NILBUF, 0);
	    break;
	}
    }
}


/* Ask server to make a cp of me. The server makes a cp between the end of the
** transaction and the sema_down. So, after a rollback, the processes is 
** re-started somewhere between the end of the transaction and the sema_down.
**  My local cp_server hears from gax, how the process arrived at sema_down.
*/
void cp_docp(me, incarno)
    capability *me;
    int incarno;
{
    header hdr1, hdr2;
    errstat n;
    
    hdr1.h_port = me->cap_port;
    hdr1.h_priv = me->cap_priv;
    hdr1.h_offset = incarno;
    hdr1.h_command = CP_CHECKPOINT;
    hdr1.h_size = 0;
    hdr1.h_signature = pub_port;
    if (NULLPORT(&pub_port)) {
	fprintf(file, "\t\tcp_docp (cpu=%d): NULLPORT", this_cpu);
    }

    n = trans(&hdr1, NILBUF, 0, &hdr2, NILBUF, 0);
    if(ERR_STATUS(n)) {
	fprintf(stderr, "%d: cp_docp: trans failed: %s\n", this_cpu,
		err_why(ERR_CONVERT(n)));
	exit(1);
    }
    sema_down(&cp_done);
}


/* Ask server to rollback me to incarno `incarno'. If the server succeeds, the
** trans will not return. 
*/
int cp_rollback(me, incarno)
    capability *me;
    int incarno;
{
    header hdr;
    errstat n;
    
    hdr.h_port = me->cap_port;
    hdr.h_priv = me->cap_priv;
    hdr.h_offset = incarno;
    hdr.h_command = CP_ROLLBACK;
    hdr.h_signature = pub_port;
    
    n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(n)) {
	fprintf(stderr, "%d: rollback: trans failed: %s\n", this_cpu,
		err_why(ERR_CONVERT(n)));
	return(0);
    }
    if(hdr.h_status != STD_OK) {
	fprintf(stderr, "%d: rollback: request returned %d\n", this_cpu, 
		hdr.h_status);
	return(0);
    }
    return(1);
}


int cp_flush(me, incarno)
    capability *me;
    int incarno;
{
    static header hdr;
    errstat n;
    
    hdr.h_port = me->cap_port;
    hdr.h_priv = me->cap_priv;
    hdr.h_offset = incarno;
    hdr.h_command = CP_FLUSH;
    hdr.h_signature = pub_port;
    
    n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(n)) {
	fprintf(stderr, "%d: cp_flush: trans failed: %s\n", this_cpu,
		err_why(ERR_CONVERT(n)));
	return(0);
    }
    if(hdr.h_status != STD_OK) {
	fprintf(stderr, "%d: flush: request returned %d: %s\n", this_cpu,
			hdr.h_status, err_why(ERR_CONVERT(hdr.h_status)));
	return(0);
    }
    return(1);
}


cp_init()
{
    thread_newthread(cp_server, stacksize, 0, 0);
}


cp_finish()
{
    static header hdr;
    errstat r;

    hdr.h_port = pub_port;
    hdr.h_command = CP_FINISH;
    r = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    if (ERR_STATUS(r))
	fprintf(stderr, "%d: cp_finish: %s\n", this_cpu, err_why(r));
}
