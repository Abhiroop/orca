#include "amoeba.h"
#include "module/mutex.h"
#include "thread.h"
#include "pan_sys.h"
#include "pan_amoeba.h"

static
struct { header *request_hdr;
	 bufptr request_buf;
	 bufsize request_size;
	 header *reply_hdr;
	 bufptr reply_buf;
	 bufsize reply_size;
	 bufsize result; } params;

static int finished = 0;
static int server_installed = 0;

static mutex server_lock, client_lock;

static pan_mutex_p trans_lock;


bufsize __trans(header *request_hdr, bufptr request_buf, bufsize request_size,
		header *reply_hdr, bufptr reply_buf, bufsize reply_size);


static void
trans_server( char * param, int psize)
{
    server_installed = 1;
    for (;;) {
	mu_lock( &server_lock);		/* wait for client */
	if (finished) {
	    thread_exit();
	}
	params.result = __trans(params.request_hdr,
				params.request_buf,
				params.request_size,
	 	 	 	params.reply_hdr,
				params.reply_buf,
				params.reply_size);
	mu_unlock( &client_lock);	/* signal client */
    }
}

void
pan_amoeba_init()
{
    trans_lock = pan_mutex_create();
    mu_init( &server_lock);
    mu_lock( &server_lock);
    mu_init( &client_lock);
    mu_lock( &client_lock);
    thread_newthread( trans_server, 64*1024, (char *)0, 0);
    threadswitch();		/* make sure (?) trans_server initializes now */
}

void
pan_amoeba_exit()
{
    finished = 1;
    server_installed = 0;
    mu_unlock( &server_lock);
}


/* Transactions may be aborted when interrupts are used. Intercept them, and
 * have a non-interruptable kernel thread do the transactions.
 */

bufsize _trans(header *request_hdr, bufptr request_buf, _bufsize request_size,
	       header *reply_hdr, bufptr reply_buf, _bufsize reply_size)
{
    bufsize res;

    if (!server_installed) {
	return __trans(request_hdr, request_buf, request_size,
		       reply_hdr, reply_buf, reply_size);
    }

    pan_mutex_lock( trans_lock);
    params.request_hdr = request_hdr;
    params.request_buf = request_buf;
    params.request_size = request_size;
    params.reply_hdr = reply_hdr;
    params.reply_buf = reply_buf;
    params.reply_size = reply_size;
    mu_unlock( &server_lock);		/* signal trans_server */
    mu_lock( &client_lock);		/* wait for trans_server */
    res = params.result;
    pan_mutex_unlock( trans_lock);
    return res;
}
