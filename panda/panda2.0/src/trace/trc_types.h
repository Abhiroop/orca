#ifndef __TRC_TYPES_H__
#define __TRC_TYPES_H__


#include <stdio.h>

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

 
#if (defined FALSE) && (defined TRUE)
 
#  ifndef FALSE
#  define FALSE 0
#  endif
#  ifndef TRUE
#  define TRUE 1
#  endif
 
typedef int boolean;
 
#else
 
typedef enum {FALSE = 0, TRUE = 1} boolean;
 
#endif


typedef struct THREAD_ID_T trc_thread_id_t, *trc_thread_id_p;

struct THREAD_ID_T {
    short int         my_pid;
    short int         my_thread;
};



typedef struct TRC_EVENT_DESCR_T trc_event_descr_t, *trc_event_descr_p;

 /* Events are formatted into this type when the event file is read. */
struct TRC_EVENT_DESCR_T {
    trc_event_t     type;
    trc_thread_id_t thread_id;
    pan_time_fix_t  t;
    size_t          usr_size;	/* Size of event type-specific data */
    char           *usr;	/* Event type-specific data */
};



typedef struct TRC_EVENT_INFO_T trc_event_info_t, *trc_event_info_p;

 /* Describes event type */
struct TRC_EVENT_INFO_T {
    size_t      usr_size;	/* Size of event type-specific data */
    int         level;		/* Log only if this >= global level */
    char       *name;		/* Symbolic name */
    char       *fmt;		/* How to read/format specific data */
    trc_event_t extern_id;	/* For renumber when events of different
				 * platforms have different name/id bindings. */
};



#define TRC_EVENT_BLOCK 256


typedef struct TRC_EVENT_LST_T trc_event_lst_t, *trc_event_lst_p;

 /* Linked list of event types. Array would exhaust... Is handled in reentrant
  * fashion. */
struct TRC_EVENT_LST_T {
    trc_event_t offset;		/* First event type in this block */
    int         num;		/* max. #event type in this block */
    trc_event_info_t info[TRC_EVENT_BLOCK];	/* Event infos */
    trc_event_lst_p next;	/* Linked list */
};
 


typedef long int	pan_time_diff_t;



					/* Different formats of event records */
					/* Thread id: in header of event data
					 *            block _or_ in each event;
					 * Times:     deltas _or_ absolute;
					 * Sorted:    - per thread
					 *            - per platform
					 *            - per world
					 */
typedef enum TRC_FORMAT_T {
    IMPLICIT_SRC	= 0x0,		/* Thread id in header, delta t's */
    EXPLICIT_SRC	= (0x1 << 0),	/* Thread id per event, absolute t's */
    THREADS_MERGED	= (0x1 << 1) | EXPLICIT_SRC,
					/* Sorted per platform */
    PLATFORMS_MERGED	= (0x1 << 2) | THREADS_MERGED,
					/* Sorted per world */
    STATE_FIRST		= (0x1 << 3),
    OBJECTS_TICKED	= (0x1 << 4),	/* Object rebind info collected? */
    EVENTS_TICKED	= (0x1 << 5),	/* Bit set of event types per thread? */
    COMPACT_FORMAT	= (0x1 << 6),	/* compress event struct in file */
    LITTLE_ENDIAN	= (0x1 << 7)	/* Replicate endianness here */
} trc_format_t, *trc_format_p;



typedef struct TRC_LST_T	trc_lst_t, *trc_lst_p;
					/* Contains chunks of memory with binary
					 * representations of events in buf.
					 * The chunks form a FIFO queue (the
					 * most recent one at the tail), one
					 * queue per thread.
					 * The queues can be flushed concurr-
					 * ently with writing of events. We
					 * need two extra pointers for this:
					 * flush_start and flush_end */
struct TRC_LST_T {
    char	       *buf;		/* The event buffer */
    char	       *current;	/* Pointer where to write next */
    char	       *flush_start;	/* Pointer where to start flush */
    char	       *flush_end;	/* Pointer where to end flush */
    size_t		size;		/* #bytes of buffer */
    trc_lst_p		next;		/* next chunk */
    FILE	       *stream;		/* Trace file: may flush on the fly */
    trc_format_t	format;		/* My trace file format */
    trc_thread_id_t     thread_id;	/* thread that writes this buffer */
    pan_time_fix_t	t_current;	/* time of last flushed elt */
};


typedef long unsigned int      *trc_event_tp_set_p;


typedef struct TRC_THREAD_INFO_T trc_thread_info_t, *trc_thread_info_p;
					/* Thread info struct; each platform
					 * has a linked list of these. */
struct TRC_THREAD_INFO_T {
    int			entries;	/* #events created by me */
    trc_lst_p		trc_tail;	/* My event buffer list: tail */
    trc_lst_p		trc_front;	/* My event buffer list: front */
    pan_time_p		t_current;	/* My latest event tick time */
    pan_time_p		t_scratch;	/* Scratch space for time */
    pan_time_fix_t      t_fix_current;	/* t_current for off-line */
    char	       *name;		/* My symbolic name */
    trc_thread_id_t     thread_id;	/* My platform number and */
					/* my thread, unique per platform */
    int			upshot_id;	/* My number, unique globally */
    int			must_flush;	/* Run out of event space? */
    trc_event_tp_set_p  event_tp_set;   /* set of event types used by me */
    trc_thread_info_p	next_thread;	/* next */
};



typedef struct PID_THREADS_T trc_pid_threads_t, *trc_pid_threads_p;
					/* Thread list per platform */
struct PID_THREADS_T {
    trc_thread_info_p	threads;	/* My thread list */
    int			n_threads;	/* Number of my threads */
};





				/* Opaque declaration of rebind struct */
typedef struct TRC_REBIND_T trc_rebind_t, *trc_rebind_p;


typedef struct TRC_T	trc_t, *trc_p;
					/* World global data. Also used per
					 * platform at tracing time */
struct TRC_T {
    pan_key_p		key;		/* Glocal key for trace things */
    int			my_pid;		/* -1 when whole world */
    int			n_pids;		/* #platforms in the world */
    size_t		max_mem_size;	/* Flush trace data when mem_size */
    size_t		mem_size;	/* exceeds max_mem_size */
    trc_event_lst_p	event_list;	/* Event types */
    int			entries;	/* #events */
    int			level;		/* Trace only events >= level */
    int			n_threads;	/* Total number of (real) threads */
    trc_pid_threads_p   thread;		/* Array of thread lists, one per
					 * platform. Virtual threads (used in
					 * merging) attributed to platform -1,
					 * which does not exist. */
    pan_time_p          t_current;	/* Undefined unless THREADS_MERGED */
    pan_mutex_p		lock;		/* Protect global trace data */
    pan_time_fix_t	t_off;		/* Clock synch time */
    pan_time_fix_t	t_d_off;	/* Clock synch stddev */
    pan_time_fix_t	t_start;	/* In local time units */
    pan_time_p          t_start_native;	/* Local, native time units */
    pan_time_fix_t	t_stop;		/* In local time units */
    char	       *filename;	/* Trace file, also symbolic name */
    trc_lst_t           out_buf;	/* Undefined unless THREADS_MERGED */
    trc_lst_t           in_buf;		/* Undefined unless THREADS_MERGED */
    trc_rebind_p        rebind;		/* to accumulate objects/operations */
    int                 n_event_types;	/* number of event types logged;
					 * only read/written if EVENTS_TICKED */
};


#define PANDA_1		1	/* Compatibility with panda1.x:
				 * reserve room in all structures for the
				 * platform numbered pan_my_pid().
				 */


#endif
