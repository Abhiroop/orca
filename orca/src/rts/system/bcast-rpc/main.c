/* $Id: main.c,v 1.20 1996/07/04 08:52:54 ceriel Exp $ */

#include <signal.h>

#include "amoeba.h"
#include "semaphore.h"
#include "caplist.h"
#include "group.h"

#include "interface.h"
#include "remote.h"
#include "scheduler.h"
#include "message.h"
#include "policy.h"
#include "server.h"

int ncpus;			/* number of cpus in application */
int this_cpu;			/* this cpu */
int counter;			/* counter for object ids */
int maxmesssize;		/* maximum message size */
int logmaxmesssize;		/* 2 log of maximum size group message */
int logmaxbuf;			/* 2 log of history size */
int checkpointing;		/* make checkpoints ? */
int cp_interval;		/* interval in sec between two checkpoints */
int replication_policy;		/* policy for replication */
int rts_verbose;		/* verbose? */
int grp_debug;			/* set debug flag for group code? */
int do_strategy = 1;		/* set if strategy calls are to be ignored */
int n_rpc_listeners;		/* number of rpc listeners */
int stacksize =	STACKSIZE;
extern int sw_protocol_sz;	/* buffer size which indicates going from
				   BB to PB */
struct ObjectEntry *ObjectTable[MAXENTRIES];	/* object table */
FILE *file;
#ifdef PROGRAM_MUST_SWITCH
int schedcount = SCHEDINIT;
#endif
int trace_reads;

#ifdef OPTIMIZED
#ifdef CHK_FORK_MSGS
/* This #define enables checksums on FORK messages. To detect "problem"
   processor in zoo pool.
*/
int chksums;
extern char *bufs[];
extern int buflens[];
#endif
#endif

extern semaphore sem_end;
extern semaphore fork_lock;
extern capability *groupcap, *me;	/* set by gax */

static int	ac;
static char	**av;


extern int optint;
extern char *optarg;

static void
usage(void)
{
    fprintf(stderr, "usage: prog cpu ncpu [-r] [-s] [-a] [-l n_rpc_listeners] [-k stacksize]\
 [-m log-max-mess-size] [-d grp-debug-flag] [-b log-n-buf] [-c interval]\
 [-p 1|2|3 ] [-t tracebufsize] [-v level] [-g protswitch]\n");
    exit(-1);
}

#ifdef sparc
#define LOGNBUF 8
#define LOG_BUF	13
#else
#define LOGNBUF	7
#define LOG_BUF	12
#endif
#define MAXTRACENAME 16
#define MAX_BUF		(1 << LOG_BUF)

/* extra group communication timer parameters */
interval
	grp_var_sync_timeout,
	grp_var_reply_timeout,
	grp_var_alive_timeout,
	grp_var_reset_timeout,
	grp_var_locate_timeout,
	grp_var_max_retrial;

extern prc_dscr	OrcaMain;
extern void	ini_OrcaMain(void);

static void
catch_abort(int signo)
{
	exit(signo);
}

int
main(int argc, char *argv[]) {
    extern int trace_on;
    capability *getcap();
    capability *filecap;
    int fd;
    int i;
    int opt;
    static char tracename[MAXTRACENAME];
    
    file = stderr;
    setvbuf(stderr, (char *) 0, _IOLBF, BUFSIZ);
    if(argc < 3) usage();
    
    this_cpu = atoi(argv[1]);
    ncpus =  atoi(argv[2]);
    logmaxmesssize = LOG_BUF;
    maxmesssize = MAX_BUF;
    logmaxbuf = 1;
    while ((ncpus << 3) < (1 << logmaxbuf)) logmaxbuf++;
    if (logmaxbuf < LOGNBUF) logmaxbuf = LOGNBUF;
    checkpointing = 0;
    n_rpc_listeners = ncpus + 5;
#ifdef OPTIMIZED
    replication_policy = REPLICATE_ALL_OR_MASTER;	/* default strategy */
#else
    replication_policy = REPLICATE_ALL;			/* default strategy */
#endif
    rts_verbose = 0;
    sprintf(tracename, "trace%d", this_cpu);
    sema_init(&fork_lock, 1);
    
    for (i = 3; i < argc; i++) {
	if (! strcmp(argv[i], "-OC")) {
		ac = argc - i;
		argv[i] = argv[0];
		av = &argv[i];
		argc = i;
		break;
	}
    }
    if (ac == 0) {
	ac = 1;
	av = &argv[0];
    }
    while((opt = getopt(argc - 2, argv + 2, "l:k:g:d:m:b:c:t:p:rasxv:S:R:A:T:L:M:")) != EOF) {
	switch(opt) {
	case 'l':
	    n_rpc_listeners = atoi(optarg);
	    break;
	case 'k':
	    stacksize = atoi(optarg);
	    break;
	case 'g':
	    sw_protocol_sz = atoi(optarg);
	    break;
	case 'c':
	    checkpointing = 1;
	    cp_interval = atoi(optarg);
	    break;
	case 'b':
	    logmaxbuf = atoi(optarg);
	    break;
	case 'm':
	    logmaxmesssize = atoi(optarg);
	    maxmesssize = 1 << logmaxmesssize;
	    break;
	case 'p':
	    replication_policy = atoi(optarg);
	    if(replication_policy < 1 || replication_policy > NPOLICY)
		usage();
	    break;
	case 't':
	    trace_init(tracename, atoi(optarg));
	    break;
	case 'a':
	    signal(SIGABRT, catch_abort);
	    break;
	case 'x':
#ifdef CHK_FORK_MSGS
	    chksums = 1;
#endif
	    break;
	case 'v':
	    rts_verbose = atoi(optarg);
	    break;
	case 'r':
	    trace_reads = 1;
	    break;
	case 's':
	    do_strategy = 0;
	    break;
	case 'd':
	    grp_debug = atoi(optarg);
	    break;
	case 'S':
	    grp_var_sync_timeout = atoi(optarg);
	    break;
	case 'R':
	    grp_var_reply_timeout = atoi(optarg);
	    break;
	case 'A':
	    grp_var_alive_timeout = atoi(optarg);
	    break;
	case 'T':
	    grp_var_reset_timeout = atoi(optarg);
	    break;
	case 'L':
	    grp_var_locate_timeout = atoi(optarg);
	    break;
	case 'M':
	    grp_var_max_retrial = atoi(optarg);
	    break;
	default:
	    usage();
	}
    }

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ); /* setlinebuf( stdout); */

    if((groupcap = getcap(GROUPCAP)) == NULL)
	m_syserr("no GROUPCAP in cap env");
    if((me = getcap(MEMBERCAP)) == NULL)
	m_syserr("no MEMBERCAP in cap env");

#ifdef notdef
    /* to make sure that I/O redirection works under UNIX */
    if((filecap = getcap("STDOUT")) == NULL)
        m_syserr("no STDOUT capability");
    if((fd = opencap(filecap, O_RDWR)) < 0)
        m_syserr("opencap failed");
    if((file = fdopen(fd, "w")) == NULL)
        m_syserr("fdopen failed");
#endif

    i = ncpus;
    counter = 0;
    while (i > 256) {
	counter += 8;
	i >>= 8;
    }
    while (i != 0) {
	i >>= 1;
	counter++;
    }
    counter++;
    counter = (this_cpu << (8 * sizeof(counter) - counter)) + 1;
    ini_OrcaMain();
    initproc();
    initcommunication();
    srand(0);    /* initialize local random generator */
    if(this_cpu == ncpus - 1) {
	fprintf(file, "Orca rts %s: cpu %s #cpu %s max-msg %d %s\n",
	       argv[0], argv[1], argv[2],
	       maxmesssize, checkpointing ?
	       "checkpointing enabled" : "");
	fflush(file);
    }
    if (this_cpu == 0) {
	/* create main orca process on processor 0 */
	DoFork(0, &OrcaMain, (void **) 0);
    }
    sema_down(&sem_end);
    endcommunication();
    if (trace_on) {
	fprintf(file, "TRACE_END CALLED on CPU %d\n", this_cpu);
	trace_end(tracename);
    }
    if(checkpointing)
	printcheckstat();
    doexit(0);
}

#ifdef CHK_FORK_MSGS
doexit(n)
    int n;
{
    if (chksums && n) {
	char buf[20];
	register int i;

	for (i = 0; i < 128; i++) {
		if (buflens[i]) {
			FILE *f;
			sprintf(buf, "%d_%d", this_cpu, i);
			f = fopen(buf, "w");
			if (f) {
				fwrite(bufs[i], 1, buflens[i], f);
				fclose(f);
			}
		}
	}
    }
    exit(n);
}
#endif

int GenerateId(void)
{
    return counter++;
}


f_Finish__Finish(void)
{
    stopgroup();
}

t_integer f_args__Argc(void)
{
  return	ac;
}

void f_args__Argv(t_integer n, t_string *s)
{
  free_string(s);
  if (n >= 0 && n < ac) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(av[n]));
	strncpy(&((char *)(s->a_data))[s->a_offset], av[n], s->a_sz);
  }
}

void f_args__Getenv(t_string *e, t_string *s)
{
  char	buf[1024];
  char	*p = buf;
  char	*ev;

  if (e->a_sz >= 1024) {
	p = m_malloc(e->a_sz + 1);
  }
  strncpy(p, &((char *)(e->a_data))[e->a_offset], e->a_sz);
  p[e->a_sz] = 0;
  ev = getenv(p);
  if (e->a_sz >= 1024) m_free(p);
  free_string(s);
  if (ev) {
	a_allocate(s, 1, sizeof(t_char), 1, strlen(ev));
	strncpy(&((char *)(s->a_data))[s->a_offset], ev, s->a_sz);
  }
}
#ifdef CHK_FORK_MSGS
int
chksum(buf, len)
    char *buf;
{
    register char *p = buf;
    register int x = 0;
    register int n;
    register int j;
    short i[100];
    
    len /= sizeof(short);

    for (n = 0; n < len; n++) {
	if ((n % 100) == 0) {
		if (len-n >= 100) {
			memcpy(i, p, 100 * sizeof(short));
			p += 100 * sizeof(short);
		}
		else {
			memcpy(i, p, (len-n) * sizeof(short));
		}
		j = 0;
	}
	x = x ^ i[j];
	j++;
    }
    return x;
}

void
abort()
{
    doexit(6);
}
#endif
