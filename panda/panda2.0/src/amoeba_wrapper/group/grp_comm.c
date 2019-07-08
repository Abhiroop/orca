#include <unistd.h>
#include <amoeba.h>
#include <module/ar.h>
#include <stderr.h>
#include <group.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include "pan_group.h"
#include "pan_sys.h"
#include "blist.h"
#include "pan_sys_amoeba_wrapper.h"

#ifdef TRACING
#include "pan_trace.h"
#endif

#define   JOIN   1
#define   LEAVE  2
#define   DATA   3
#define   STOP   4
#define   INIT   5

/* often used amoeba header fields */
#define   COM    h_command
#define   PID    h_extra
#define   GID    h_offset
#define   LEN    h_size
#define   ADR    h_port

#define   MSGPOOL_SIZE 8

/* #define   MAX(x,y) (((x) > (y)) ? (x) : (y)) */

struct PAN_GROUP_T {
  pan_mutex_p lock;
  pan_cond_p  cond;
  pan_pset_p  members;
  int         size;
  void (*receiver) (pan_msg_p msg);
  int         installed;
  int         gid;
};

#ifdef KOEN
static pan_time_p start, stop, total;
static int nr_timings = 0;
static int valid = 0;
#endif


/****** Koen: get rid of generic t_table for performance ******/

#define MAX_ENTRIES	16

typedef unsigned long key_t;
typedef pan_group_p   data_t;
typedef data_t        table_t, *table_p;
 
static void t_create(table_p *table)
{
	int i;
	table_p tab;

	*table = tab = (table_p) sys_malloc( MAX_ENTRIES * sizeof(table_t));
	for ( i=0; i < MAX_ENTRIES; i++)
		tab[i] = NULL;
}

static void t_destroy(table_p table)
{
	free( table);
}

static data_t t_lookup(table_p table, key_t key)
{
	assert( table[key] != NULL);

	return table[key];
}

static int t_del(table_p table, key_t key, data_t *data)
{
	assert( table[key] != NULL);

	*data = table[key];
	table[key] = NULL;
	return 1;
}

static void t_add(table_p table, key_t key, data_t data)
{
	if ( key >= MAX_ENTRIES) {
		sys_panic( "too many table entries (key = %d)", key);
	}
	assert( table[key] == NULL);
	table[key] = data;
}

/***************************************************/


static g_id_t       g_id;
static port         grp_port;
static pan_thread_p grp_daemon;
static table_p      table;
static b_list_p     bindings;
static pan_mutex_p  lock;
static pan_cond_p   cond;
static int          group_id;

static int          lognbuf  = 5;
static int          bb_large = 1250;
static unsigned int sync_timeout = 20000;
static unsigned int send_timeout = 2000;


static int twolog( int n)
{
	int log;

	for ( log=0; n>1; log++)
		n >>= 1;
	return log;
}


static pan_group_p gcreate(void)
{
     pan_group_p g;

     g = (pan_group_p) sys_malloc(sizeof(pan_group_t));

     g->lock = pan_mutex_create();
     g->cond = pan_cond_create( g->lock);
     g->members = pan_pset_create();
     g->receiver = (void *) 0;
     g->installed = 0;
     g->size = 0;
     g->gid = group_id++;

     return g;
}

static void gdestroy(pan_group_p g)
{
     pan_cond_clear(g->cond);
     pan_mutex_clear(g->lock);
     pan_pset_clear(g->members);
     free(g);
}


static void make_hdr(header *hdr, int com, int len, int gid)
{
     hdr->COM = com;
     hdr->ADR = grp_port;
     hdr->PID = pan_my_pid();
     hdr->GID = gid;
     hdr->LEN = len;
}


static void send_msg(header *hdr, char *msg)
{
     int ret;

#ifdef KOEN
     if (valid) {
    	pan_time_get(stop);
    	pan_time_sub(stop, start);
    	pan_time_add(total,stop);
    	nr_timings++;
	valid = 0;
     }
#endif
     if (hdr->COM == STOP)
         ret = grp_leave(g_id, hdr);
     else
         ret = grp_send(g_id, hdr, msg, hdr->LEN);

     if (ERR_STATUS(ret)) {
         if (hdr->COM == STOP) {
             sys_panic("grp_leave failed: %s", err_why(ERR_CONVERT(ret)));
         }
         else {
             sys_panic("grp_send failed: %s", err_why(ERR_CONVERT(ret)));
         }
     }
}

static void handle_join(header hdr, pan_msg_p msg)
{
    pan_group_p grp;
    int      gid;
    void     *p;


    p = pan_msg_pop(msg, hdr.LEN, 1);
#ifdef VERBOSE
    Printf("joining group %s\n", (char *) p);
#endif

    pan_mutex_lock(lock);
    if (!b_find(bindings, (char *) p, &gid)) {
        grp = gcreate();
        b_enter(bindings, (char *) p, grp->gid);
        t_add(table, grp->gid, (data_t) grp);
#ifdef VERBOSE
        Printf("broadcast on cond %lx\n", &cond);
#endif
        pan_cond_broadcast(cond);
    }
    else {
        if ((grp = t_lookup(table, gid)) == NULL) {
            sys_panic("cannot find group %d", gid);
        }
    }
    pan_mutex_unlock(lock);

    pan_mutex_lock(grp->lock);
    pan_pset_add(grp->members, hdr.PID);
    grp->size++;
#ifdef VERBOSE
    Printf("broadcast on cond %lx\n", grp->cond);
#endif
    pan_cond_broadcast(grp->cond);
    if (hdr.PID == pan_my_pid())
        while (!grp->installed)
           pan_cond_wait(grp->cond);
    pan_mutex_unlock(grp->lock);

    pan_msg_clear(msg);
}


static void handle_leave(header hdr, pan_msg_p msg)
{
    pan_group_p grp;

    if ((grp = t_lookup(table, hdr.GID)) == NULL) {
        sys_panic("%d illegal gid", hdr.GID);
    }

    pan_mutex_lock(grp->lock);
    pan_pset_del(grp->members, hdr.PID);
    grp->size--;
    if ( hdr.PID == pan_my_pid()) {
	grp->receiver = NULL;	    /* so no more messages will be delivered */
#ifdef VERBOSE
    	Printf("cond broadcast %lx\n", &grp->cond);
#endif
    	pan_cond_broadcast(grp->cond);
    }
    pan_mutex_unlock(grp->lock);

    pan_msg_clear(msg);
}


static void handle_stop(header hdr, pan_msg_p msg)

{
    pan_mutex_lock(lock);
    pan_cond_signal(cond);
    pan_mutex_unlock(lock);

    pan_msg_clear(msg);
    if (hdr.PID == pan_my_pid()) {
        pan_thread_exit();
    }
}


static void handle_data(header hdr, pan_msg_p msg)

{
    pan_group_p grp;
    void (*upcall) (pan_msg_p msg) = NULL;

    if ((grp = t_lookup(table, hdr.GID)) == NULL) {
        pan_msg_clear(msg);
        Printf("data discarded\n");
    }

    /* pan_mutex_lock(grp->lock); 	Koen: perf hack */
    /* Test on membership no longer necessary since grp->receiver is set to
     * NULL on group leave.
     *
    if (pan_pset_ismember(grp->members, pan_my_pid()))
     */
        upcall =  grp->receiver;
    /* pan_mutex_unlock(grp->lock); */

    if (upcall) {
#ifdef VERBOSE
        Printf("upcall %d bytes\n", pan_msg_data_len(msg));
#endif
        upcall(msg);
    }
    else {
#ifdef VERBOSE
        Printf("no upcall\n");
#endif
        pan_msg_clear(msg);
    }
}

static void handle_init(header hdr, pan_msg_p msg)

{
    pan_mutex_lock(lock);
    pan_cond_signal(cond);
    pan_mutex_unlock(lock);

    pan_msg_clear(msg);
}

static void dispatch(header hdr, pan_msg_p msg)
{
    switch (hdr.COM) {
    case JOIN  : handle_join(hdr, msg);
                 break;
    case LEAVE : handle_leave(hdr, msg);
                 break;
    case DATA  : handle_data(hdr, msg);
                 break;
    case STOP  : handle_stop(hdr, msg);
                 break;
    case INIT  : handle_init(hdr, msg);
                 break;
    default    : sys_panic("unknown group message");
    }
}


static void grp_comm_daemon(void *arg)
{
    int rlen, more;
    pan_msg_p msg;
    void *p;
    header hdr;

#ifdef TRACING
    trc_new_thread(0, "group daemon");
#endif

    for (;;) {
         msg = pan_msg_create();
         p   = pan_msg_push(msg, SYS_DEFAULT_MESSAGE_SIZE, 1);

         hdr.ADR = grp_port;
         if ((rlen = grp_receive(g_id, &hdr, p, SYS_DEFAULT_MESSAGE_SIZE, &more)) < 0) {
             sys_panic("grp_receive failed: %s", err_why(ERR_CONVERT(rlen)));
         }
#ifdef KOEN
        pan_time_get(start);
	valid = 1;
#endif


         if (hdr.LEN != rlen) {
             sys_panic("message truncated from %d to %d bytes", hdr.LEN, rlen);
         }
         pan_msg_truncate(msg, rlen);
         dispatch(hdr, msg);
    }
}


static int grp_size(void)
{
    int         ret;
    grpstate_t  grpstate;
    g_indx_t    memlist[256];

    ret = grp_info(g_id, &grp_port, &grpstate, memlist, 256);
    if (ERR_STATUS(ret)) {
        sys_panic("grp_info failed: %s", err_why(ERR_CONVERT(ret)));
    }
    return grpstate.st_total;
}


void pan_group_init(void)
{
    capability *cap;
    header      hdr;
    int         ret;

#ifdef KOEN
    start = pan_time_create();
    stop = pan_time_create();
    total = pan_time_create();
    pan_time_set(total, 0, 0);
#endif
    if ((cap = (capability *) getcap("GROUPCAP")) == NULL) {
        sys_panic("getcap failed");
    }

    grp_port = cap->cap_port;
#ifdef VERBOSE
    Printf("groupcap %s\n", ar_port(&grp_port));
#endif

    if (pan_my_pid() == 0) {
        if ((g_id = grp_create(&grp_port, 0, MAX(2, pan_nr_platforms()), lognbuf,
			       twolog(2*SYS_DEFAULT_MESSAGE_SIZE-1))) < 0) {
            sys_panic("grp_create failed: %s\n", err_why(ERR_CONVERT(g_id)));
        }
    }
    else {
        sleep(10);
        make_hdr(&hdr, INIT, 0, 0);
        if ((g_id = grp_join(&hdr)) < 0) {
            sys_panic("grp_join failed: %s\n", err_why(ERR_CONVERT(g_id)));
        }
    }

    ret = grp_set(g_id, &grp_port, sync_timeout, send_timeout, 0, bb_large);
    if (ERR_STATUS(ret)) {
         sys_panic("grp_set: %d", err_why(ERR_CONVERT(ret)));
    }

    t_create(&table);
    b_create(&bindings);
    lock = pan_mutex_create();
    cond = pan_cond_create( lock);

    grp_daemon = pan_thread_create(grp_comm_daemon,
                      (void *) 0, 0L, pan_thread_maxprio() - 1, 0);

    pan_mutex_lock(lock);
    while (grp_size() != pan_nr_platforms())
           pan_cond_wait(cond);
    pan_mutex_unlock(lock);

#ifdef VERBOSE
    Printf("group created and joined by all\n");
#endif
}


void pan_group_end(void)
{
    header hdr;
    int    i;
    pan_group_p grp;

#ifdef KOEN
    if (nr_timings>0)
    	printf( "%d: total response time %f in %d ticks, per message %f\n",
                pan_my_pid(), pan_time_t2d(total), nr_timings,
		pan_time_t2d(total)/nr_timings);
    pan_time_clear(start);
    pan_time_clear(stop);
    pan_time_clear(total);
#endif
    if (pan_my_pid() == 0) { /* i am the sequencer */
        pan_mutex_lock(lock);
        while (grp_size() != 1)
               pan_cond_wait(cond);
        pan_mutex_unlock(lock);
    }

    /* send suicide message to the group daemon */
    make_hdr(&hdr, STOP, 0, 0);
    send_msg(&hdr, (char *) 0);

    pan_thread_join(grp_daemon);

    pan_cond_clear(cond);
    pan_mutex_clear(lock);

    for (i = 0; i < group_id; i++) {
         if (!t_del(table, i, (data_t *) &grp)) {
             sys_panic("t_del group %d doesn't exist", i);
         }
         else {
             gdestroy(grp);
#ifdef VERBOSE
             Printf("group %d destroyed\n", i);
#endif
         }
    }

    t_destroy(table);
    b_destroy(bindings);
}


pan_group_p pan_group_join(char *group_name,
                          void (*receive)(pan_msg_p msg_in))
{
    header    hdr;
    pan_group_p  grp;
    int       gid;
    pan_msg_p msg;
    void      *p;

    msg = pan_msg_create();
    p   = pan_msg_push(msg, strlen(group_name)+1, 1);

    sprintf(p, "%s", group_name);
    make_hdr(&hdr, JOIN, pan_msg_data_len(msg), 0);
    send_msg(&hdr, p);
#ifdef VERBOSE
    Printf("sent group name is %s len is %d\n", p, hdr.LEN);
#endif

    pan_mutex_lock(lock);
    while (!b_find(bindings, p, &gid)) {
#ifdef VERBOSE
           Printf("waiting on cond %lx\n", &cond);
#endif
           pan_cond_wait(cond);
    }
    pan_mutex_unlock(lock);

    if ((grp = t_lookup(table, gid)) == NULL) {
         sys_panic("cannot find gid %d", gid);
    }

    pan_mutex_lock(grp->lock);
    while (!pan_pset_ismember(grp->members, pan_my_pid())) {
#ifdef VERBOSE
           Printf("waiting on cond %lx\n", grp->cond);
#endif
           pan_cond_wait(grp->cond);
    }
    grp->receiver = receive;
    grp->installed = 1;
    pan_cond_signal(grp->cond);
    pan_mutex_unlock(grp->lock);
    pan_msg_clear(msg);

    return grp;
}


void pan_group_leave(pan_group_p g)
{
    header   hdr;

    pan_mutex_lock(g->lock);
    if (!pan_pset_ismember(g->members, pan_my_pid())) {
        sys_panic("leaving group %d, but not a member");
    }
    pan_mutex_unlock(g->lock);

    make_hdr(&hdr, LEAVE, 0, g->gid);
    send_msg(&hdr, (char *) 0);

    pan_mutex_lock(g->lock);
    while (pan_pset_ismember(g->members, pan_my_pid())) {
           pan_cond_wait(g->cond);
    }
    g->installed = 0;
    pan_mutex_unlock(g->lock);
}


void pan_group_clear(pan_group_p g)
{
}


void pan_group_await_size(pan_group_p g, int size)

{
    pan_mutex_lock(g->lock);
    while (g->size < size) {
           pan_cond_wait(g->cond);
    }
    pan_mutex_unlock(g->lock);

#ifdef VERBOSE
    Printf("pan_group_await_size %d\n", size);
#endif
}


void pan_group_send(pan_group_p g, pan_msg_p msg)
{
    header hdr;

#ifdef VERBOSE
    Printf("grp_send %d bytes\n", pan_msg_data_len(msg));
#endif
    make_hdr(&hdr, DATA, pan_msg_data_len(msg), g->gid);
    send_msg(&hdr, (char *) pan_msg_look( msg, hdr.LEN, 1));
    pan_msg_clear(msg);
}


void           pan_group_va_set_params(void *first, ...)
{
    va_list  args;
    char    *cmd;
    int      buf_size;
    int      hist_size;

    va_start(args, first);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if        (strcmp("PAN_GRP_bb_large",            cmd) == 0) {
	    bb_large            = va_arg(args, int);
	} else if (strcmp("PAN_GRP_hist_size",           cmd) == 0) {
	    hist_size           = va_arg(args, int);
	    buf_size = 1;
	    lognbuf = 0;
	    while (buf_size < hist_size) {
		buf_size *= 2;
		++lognbuf;
	    }
	} else if (strcmp("PAN_GRP_unord_buf_size",       cmd) == 0 ||
		   strcmp("PAN_GRP_ord_buf_size",         cmd) == 0) {
	    /* ignore, only relevant for rawflip version */
	    va_arg(args, void*);
	} else {
	    if (pan_my_pid() == 0) {
	    	Printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    }
	    va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}

void pan_group_va_set_g_params(pan_group_p g, ...)
{
    Printf( "pan_group_va_set_g_params(): not implemented; options ignored\n");
}


void pan_group_va_get_g_params(pan_group_p g, ...)
{
    va_list args;
    char    *cmd;

    va_start(args, g);

    cmd = va_arg(args, char *);
    while (cmd != NULL) {

	if        (strcmp("PAN_GRP_sequencer",   cmd) == 0) {
	    *(int*)va_arg(args, int*) = 0;
	} else if (strcmp("n_members",           cmd) == 0) {
	    pan_mutex_lock(g->lock);
	    *(int*)va_arg(args, int*) = g->size;
	    pan_mutex_unlock(g->lock);
	} else {
	    if (pan_my_pid() == 0) {
	    	Printf("No such group value tag: \"%s\" -- ignored\n", cmd);
	    }
	    va_arg(args, void*);
	}

	cmd = va_arg(args, char *);
    }

    va_end(args);
}
