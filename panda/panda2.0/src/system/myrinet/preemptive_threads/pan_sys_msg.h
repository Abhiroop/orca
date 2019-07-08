#ifndef __PANDA_SYSTEM_LAYER__
#define __PANDA_SYSTEM_LAYER__

/* Koen: local copy so we can inline mutexes in all interface layer modules
 */

/*
   This header file defines the Panda system layer interface, that puts
   an abstract layer on top of an existing operating system. The
   implementation of the system layer must provide the functions
   presented here WITHOUT MODIFYING THIS FILE!!! This way it is
   guaranteed that all implementations on top of it are architecture
   independent. All higher layers will only include this file.

   Naming convention: All names that are externally visible have to be
   prefixed with '{\em pan}'. The names that are supposed not to be used
   by the higher layers start with '{\em pan\_sys\/}'. All modules
   layered on top of the system module should have names like '{\em
     pan\_xxx\_name}', where {\em xxx} is the unique name of the module.
*/


/*
   TODO:
   \begin{itemize}
   \item Think about the effects of an extra procedure call for mutexes.
   \item Lock management when calling pan\_send.
   \item Lock management during upcalls (wrt active messages?)
   \item Add pan\_msg\_data\_push and pan\_msg\_data\_pop.
   \end{itemize}
*/

/* 
\section{Type Definitions} 

All Panda system interface type are pointers to structures. This allows an
implementation to provide its own types, without requiring these types to be
known to the upper layers. All types have there own {\em create} and 
{\em clear} routines.
*/

typedef struct pan_thread   *pan_thread_p;
typedef struct pan_time     *pan_time_p;
typedef struct pan_mutex    *pan_mutex_p;
typedef struct pan_cond     *pan_cond_p;
typedef struct pan_msg      *pan_msg_p;
typedef struct pan_pset     *pan_pset_p;
typedef struct pan_nsap     *pan_nsap_p;
typedef struct pan_key      *pan_key_p;
typedef void               (*pan_thread_func_p)(void *arg);


/*
   \section{System Interface} 

   The system interface provides the basic functions to start and stop
   the application, and to get information on the number of platforms and
   the own platform identifier. It also provides a function to poll the
   system layer for asynchronous events. This function is architecture
   dependent, and should only be called when the documentation of the
   system layer implementation says it is necessary for the correct
   functioning of the system. This implies that it cannot be called from
   general module implementations.
*/

extern void pan_init(int *argc, char *argv[]); 
/*
   Initializes the Panda layer and joins the platforms that run the same
   application. The argument list must be passed directly from the main
   function. After this call, the main function can parse its own
   arguments. The arguments used by Panda are removed from the list.
*/

extern void pan_end(void);
/*
   Releases all resources held by the system layer.
*/

extern void pan_start(void);
/*
   Starts the receive daemon. Before this call is made, all network
   service access points have to be initialized. After this call,
   incoming messages will be handled.

*/

extern int pan_nr_platforms(void);
/*
   Returns the number of platforms.
*/

extern int pan_my_pid(void);
/*
   Returns the platform identification number for this platform. The
   number will always be in the range of 0 up to pan\_nr\_platforms().
*/

extern void pan_poll(void);
/*
   Allows the system layer to synchronously handle asynchronous events.
*/

/*
   \section{Threads Interface} 

   The threads interface provides a Cthreads/pthreads alike thread
   interface.  A thread data type gets an thread of control with {\em
     pan\_thread\_create}.
*/

extern pan_thread_p pan_thread_create(pan_thread_func_p func,
				      void *arg, long stacksize, 
				      int priority, int detach);
/*
   Create a new thread, and start an execution which calls function {\em
   func} with argument {\em arg}. The stack size and priority of the new
   thread are set to {\em stacksize} and {\em priority}. A value of 0
   denotes the default value for both the stack size. The default
   stack size is architecture dependent. The {\em detach} argument
   specifies whether a thread will be joined in the future or not. A 0
   means that the user will join this thread; a 1 means that the thread
   resources can be removed on exit.
*/

extern void pan_thread_clear(pan_thread_p thread);
/*
   Clear thread {\em thread}. This function may only be called when the
   thread of control associated with {\em thread\/} is ended.
*/

extern void pan_thread_exit(void);
/*
   Terminates the current execution. This function must be called when a
   thread exits from its main function. XXX: we could also move the
   detach flag to this function.
*/

extern void pan_thread_join(pan_thread_p thread);
/*
   Join the execution associated with {\em thread}. The call blocks until
   the corresponding execution calls pan\_thread\_exit. The execution
   associated with {\em thread\/} must have been started with {\em
     detach\/} value 0.
*/

extern void pan_thread_yield(void);
/*
   Tries to run another runnable thread with equal priority. The calling
   thread will be halted and made runnable, so that it may be
   rescheduled.
*/

extern pan_thread_p pan_thread_self(void);
/*
   Returns the thread pointer of the calling thread.
*/

extern int pan_thread_getprio(void);
/*
   Returns the priority of the calling thread.
*/

extern int pan_thread_setprio(int priority);
/*
   Changes the priority of the calling thread to {\em priority}. It
   returns the old priority of the thread.
*/

extern int pan_thread_minprio(void);
/*
   Returns the minimum priority a thread may have.
*/

extern int pan_thread_maxprio(void);
/*
   Returns the maximum priority a thread may have.
*/

/* 
\section{Time Interface} 

The time interface provides a common abstraction to the system time. A
user can only create a new absolute time by taking the current time and
adding a relative time to it.
*/

extern pan_time_p pan_time_create(void);
/*
   Create a new time structure.
*/

extern void pan_time_clear(pan_time_p time);
/*
   Clears time structure {\em time}.
*/

extern void pan_time_copy(pan_time_p to, pan_time_p from);
/*
   Copy time value of {\em from} into {\em to}.
*/

extern void pan_time_get(pan_time_p now);
/*
   Get the current time in time structure {\em time}.
*/

extern void pan_time_set(pan_time_p time, long seconds, 
			 unsigned long nanoseconds);
/*
   Set time structure {\em time} to {\em seconds} seconds and {\em
   nanoseconds} nanoseconds.
*/

extern int pan_time_cmp(pan_time_p t1, pan_time_p t2);
/*
   Compares time structures {\em t1} and {\em t2}. Returns -1 if {\em
   t1\/} $<$ {\em t2\/}, 1 if {\em t1\/} $>$ {\em t2\/}, and 0 if
   {\em t1\/} $=$ {\em t2\/}.
*/

extern void pan_time_add(pan_time_p res, pan_time_p delta);
/*
   Add time {\em delta} to {\em res}.
*/

extern void pan_time_sub(pan_time_p res, pan_time_p delta);
/*
   Substract time {\em delta} from {\em res}.
*/

extern void pan_time_mul(pan_time_p res, int nr);
/*
   Multiply time {\em res} by {\em nr}.
*/

extern void pan_time_div(pan_time_p res, int nr);
/*
   Divide time {\em res} by {\em nr}.
*/

extern void pan_time_mulf(pan_time_p res, double nr);
/*
   Multiply time {\em res} by {\em nr}.
*/


extern void pan_time_d2t(pan_time_p t, double d);
/*
   Convert {\em double d} (in seconds) to {\em pan\_time\_p t}.
*/

extern double pan_time_t2d(pan_time_p t);
/*
   Convert {\em pan\_time\_p t} to seconds.
*/

typedef struct pan_time_fix pan_time_fix_t, *pan_time_fix_p;

struct pan_time_fix {
    long int          t_sec;
    long int          t_nsec;
};
/*
   Explicit time type, to be used in trace logs
*/

void pan_time_t2fix(pan_time_p t, pan_time_fix_p f);
/*
   Conversion from opaque time to explicit time.
*/

void pan_time_fix2t(pan_time_fix_p f, pan_time_p t);
/*
   Conversion from explicit time to opaque time.
*/

int  pan_time_sub_fix_small(pan_time_p t1, pan_time_p t2, long int *d);
/*
   Subtract to opaque time values, and if the difference in explicit time
   fits in one long int, store the difference in {\em d} and return true.
*/

extern pan_time_p pan_time_infinity;
extern pan_time_p pan_time_zero;
/*
   t = $\inf$ and t = 0, respectively
*/

extern int pan_time_size(void);
/*
   Returns the size of a marshalled time.
*/

extern void pan_time_marshall(pan_time_p p, void *buffer);
/*
   Marshalls time {\em p} in {\em buffer}. {\em buffer} should
   be at least {\em pan\_time\_size bytes}.
*/

extern void pan_time_unmarshall(pan_time_p p, void *buffer);
/*
   Unmarshalls {\em buffer} to time {\em p}.
*/


/* 
\section{Monitor Interface} 

*/

extern pan_mutex_p pan_mutex_create(void);
/*
   Create a new mutex.
*/

extern void pan_mutex_clear(pan_mutex_p mutex);
/*
   Clear mutex {\em mutex}.
*/

extern void pan_mutex_lock(pan_mutex_p mutex);
/*
   Tries to lock {\em mutex} and blocks until it succeeds. Acquiring a
   lock twice before releasing it by the same thread may result in a
   deadlock.
*/

extern void pan_mutex_unlock(pan_mutex_p mutex);
/*
   Unlocks {\em mutex}, and gives other threads the opportunity to lock
   it.
*/

extern int pan_mutex_trylock(pan_mutex_p mutex);
/*
   Tries to lock {\em mutex}. It returns 1 if it succeeds, 0 if {\em
   mutex} is already locked.
*/

extern pan_cond_p pan_cond_create(pan_mutex_p mutex);
/*
   Create a new condition variable belonging to monitor {\em mutex\/}.
*/

extern void pan_cond_clear(pan_cond_p cond);
/*
   Clear condition variable {\em cond}.
*/

extern void pan_cond_wait(pan_cond_p cond);
/*
   Blocks on condition variable {\em cond}. It atomically unlocks the
   associated mutex, which should have been locked by the calling thread.
   After being signaled, {\em mutex} is locked again by the calling
   thread, and the calling thread is unblocked.
*/

extern int pan_cond_timedwait(pan_cond_p cond, pan_time_p abstime);
/*
   Blocks on condition variable {\em cond}. It atomically unlocks the
   associated mutex, which should have been locked by the calling thread.
   After being signaled or after absolute time has passed {\em abstime},
   {\em mutex} is locked again by the calling thread, and the calling
   thread is unblocked. On being signaled, pan\_cond\_timedwait returns
   1, on timeout it returns 0.
*/

extern void pan_cond_signal(pan_cond_p cond);
/*
   Unblocks at least one of the threads that are blocked on {\em cond}.
   If no threads are blocked on {\em cond} nothing happens. The
   associated mutex must be locked.
*/

extern void pan_cond_broadcast(pan_cond_p cond);
/*
   Like pan\_cond\_signal, except that it unblocks all threads that are
   blocked on {\em cond}. The associated mutex must be locked.
*/

/* 
\section{Message Interface} 

The message interface provides a high-level abstraction to messages. It
assumes the underlying architecture provides fragmentation/assembly.

*/

#define alignof(type) (sizeof(struct{char i; type c;}) - sizeof(type))

typedef void (*pan_msg_release_f)(pan_msg_p msg, void *arg);

extern pan_msg_p pan_msg_create(void);
/*
   Create a new, empty message.
*/

extern void pan_msg_clear(pan_msg_p msg);
/*
   If there are release functions associated with this message, the last
   release function will be popped from the release function stack and
   called. Otherwise, the message {\em msg\/} will be cleared.
*/

extern void pan_msg_empty(pan_msg_p msg);
/*
   Changes the contents of message {\em msg\/} to empty. All pointers to
   data in the message become invalid. The release functions are
   preserved.
*/

extern void *pan_msg_push(pan_msg_p msg, int length, int align);
/*
   Prepends a buffer of size {\em length} to message {\em msg}. The
   buffer is guaranteed to be aligned by {\em align} bytes. Returns a
   pointer to the newly allocated buffer.
*/

extern void *pan_msg_pop(pan_msg_p msg, int length, int align);
/*
   Removes a buffer of size {\em length} from the head of message {\em
   msg}. The buffer is guaranteed to be aligned by {\em align} bytes.  A
   pointer to the buffer is returned.
*/

extern void *pan_msg_look(pan_msg_p msg, int length, int align);
/*
   Returns a pointer to a buffer of size {\em length} at the head of
   message {\em msg}. The buffer is guaranteed to be aligned by {\em
   align} bytes.
*/

extern void pan_msg_copy(pan_msg_p msg, pan_msg_p copy);
/*
   Copies the contents of {\em msg} to {\em copy}. All existing pointers
   to data in {\em copy\/} become invalid. The release functions are
   preserved.
*/

extern void pan_msg_release_push(pan_msg_p msg, 
				 pan_msg_release_f func,
				 void *arg);
/*
   Adds release function {\em func\/} to message {\em msg}. It will be
   called when {\em pan\_msg\_clear} is called. The message {\em msg\/}
   and argument {\em arg} are passed to {\em func} when {\em func\/} is
   called.  It is the responsibility of the release function to clear
   {\em msg\/}.
*/

extern void pan_msg_release_pop(pan_msg_p msg);
/*
   Removes the last-pushed release function of message {\em message}.
*/

/*
   \section{Network Service Access Point Interface} 

   To allow multiple higher-level modules to be added to an application,
   Panda uses a two level naming scheme. The first level is the platform
   identifier (or platform set for multicast operations), which specifies
   the destination platform(s). The second level is the network service
   access point, which identifies the service a msg is destinated
   for. Every higher-level module that does communication creates one or
   more access points (one for each type of header it uses). To
   facilitate addressing, all network service access points have to be
   acquired before {\em pan\_start} is called, so all access points are
   acquired in the same order on all platforms. The nsap specifies the
   upcall function to be used, and the size of the header.  To allow
   efficient implementations of small messages, a special type of nsap is
   introduced that only allows data of a limited size ({\em
     MAX\_SMALL\_SIZE}) to be sent.  XXX: at the moment, it is not
   possible to forward a received msg via a different nsap, so the
   type of the header cannot change.
*/

#define MAX_HEADER_SIZE 32		/* For myrinet; used to be 128 */
#define MAX_SMALL_SIZE  16

#define PAN_NSAP_UNICAST   0x01
#define PAN_NSAP_MULTICAST 0x02

extern pan_nsap_p pan_nsap_create(void);
/*
   Create a new network service access point.
*/

extern void pan_nsap_clear(pan_nsap_p nsap);
/* 
   Clear network service access point {\em nsap}.
*/

extern void pan_nsap_msg(pan_nsap_p nsap, void rec_msg(pan_msg_p msg), 
			 int flags);
/*
   Initialize {\em nsap} to a network service access point for msgs.
   Messages received on this nsap
   are handled with an upcall to {\em rec\_msg}. {\em rec\_msg} gets as
   argument a pointer to the received message.
   The received message becomes the property of the receiving
   layer, and it must be cleared in the upcall.
   {\em flags\/} specifies the type of
   communication {\em nsap\/} is going to be used for
   (PAN\_NSAP\_UNICAST, PAN\_NSAP\_MULTICAST).
*/

extern void pan_nsap_small(pan_nsap_p nsap, void rec_small(void *data),
			   int len, int flags);
/*
   Initialize {\em nsap} to a network access point for small data
   messages. Data received on this nsap are handled with an upcall to
   {\em rec\_small}. {\em rec\_small} gets as argument a pointer to the
   received data. Data sent via {\em nsap\/} is {\em len\/} bytes long.
   {\em flags\/} specifies the type of communication {\em nsap\/} is
   going to be used for (PAN\_NSAP\_UNICAST, PAN\_NSAP\_MULTICAST).
*/

/* 
\section{Platform Set Interface} 

Platform sets are used to address multiple platforms for multicast operations.

*/

extern pan_pset_p pan_pset_create(void);
/*
   Create a new platform set.
*/

extern void pan_pset_clear(pan_pset_p pset);
/*
   Clear platform set {\em pset}.
*/

extern int pan_pset_isempty(pan_pset_p pset);
/*
   Returns 1 if platform set {\em pset} is empty, 0 otherwise.
*/

extern void pan_pset_add(pan_pset_p pset, int pid);
/*
   Add platform {\em pid} to platform set {\em pset}.
*/

extern void pan_pset_del(pan_pset_p pset, int pid);
/*
   Delete platform {\em pid} from platform set {\em pset}.
*/

extern void pan_pset_fill(pan_pset_p pset);
/*
   Changes platform set {\em pset} to a platform set that contains all
   platforms (0 up to pan\_nr\_platforms()).
*/

extern int pan_pset_find(pan_pset_p pset, int pid_offset);
/*
   Return the first platform identifier in {\em pset\/}, starting from
   platform identifier {\em pid\_offset\/}. If no platform identfier is
   found, -1 is returned.
*/

extern void pan_pset_empty(pan_pset_p pset);
/*
   Changes platform set {\em pset} to a platform set that contains none
   of the platforms.
*/

extern void pan_pset_copy(pan_pset_p pset, pan_pset_p copy);
/*
   Makes a copy of platform set {\em pset} in platform set {\em copy}.
*/

extern int pan_pset_ismember(pan_pset_p pset, int pid);
/*
   Returns 1 if platform {\em pid} is in platform set {\em pset}, 0
   otherwise.
*/

extern int pan_pset_size(void);
/*
   Returns the size of a marshalled platform set.
*/

extern void pan_pset_marshall(pan_pset_p pset, void *buffer);
/*
   Marshalls platform set {\em pset} in {\em buffer}. {\em buffer} should
   be at least {\em pan\_pset\_size bytes}.
*/

extern void pan_pset_unmarshall(pan_pset_p pset, void *buffer);
/*
   Unmarshalls {\em buffer} to platform set {\em pset}.
*/

extern int pan_pset_nr_members(pan_pset_p pset);
/*
   Returns the number of members of platform set {\em pset}
*/

/*
   \section{Communication Interface}

   The communication interface provides 2 types of send functions, each
   type having both a unicast and a multicast version. The first type
   sends msg to the nsap associated with the msg. The second
   type sends small data messages to the nsap specified in the call.
*/


extern void pan_comm_unicast_msg(int dest, pan_msg_p msg, pan_nsap_p nsap);
/*
   Send msg {\em msg\/} to destination {\em dest\/}. The msg
   is sent to the network service access point {\em nsap}.
*/

extern void pan_comm_multicast_msg(pan_pset_p pset, pan_msg_p msg,
				   pan_nsap_p nsap);
/*
   Send msg {\em msg\/} to the destinations specified in platform
   set {\em pset\/}. The msg is sent to the network service access
   point {\em nsap}.
*/

extern void pan_comm_unicast_small(int dest, pan_nsap_p nsap,
				   void *data);
/*
   Send data {\em data} on nsap {\em nsap} to destination {\em dest\/}.
   {\em nsap} should be of type small. The length of {\em data\/} is
   specified in the initialization of {\em nsap\/}.
*/

extern void pan_comm_multicast_small(pan_pset_p pset, 
				     pan_nsap_p nsap, void *data);
/*
   Send data {\em data} on nsap {\em nsap} to the destinations specified
   in platform set {\em pset\/}. {\em nsap} should be of type small. The
   length of {\em data\/} is specified in the initialization of {\em
     nsap\/}.
*/






/*
\section{Malloc Interface} 

 Multithread safe versions of these routines should be provided by the
 system layer. Furthermore, we provide Panda versions that check for
 memory overflow.
 */

#include <stdlib.h>

void *pan_malloc(size_t size);
void *pan_calloc(size_t nelem, size_t elsize);
void *pan_realloc(void *ptr, size_t size);
void  pan_free(void *ptr);


/* 
\section{Glocal Memory Interface} 

*/

extern pan_key_p pan_key_create(void);
/*
   Create a new glocal memory key
*/

extern void pan_key_clear(pan_key_p key);
/*
   Clear glocal memory key {\em key}
*/

extern void pan_key_setspecific(pan_key_p key, void *ptr);
/*
   Set the thread specific data of the current thread corresponding with
   key {\em key} to {\em ptr}.
*/

extern void *pan_key_getspecific(pan_key_p key);
/*
   Retrieve the thread specific data of the current thread corresponding
   with key {\em key}.
*/


/*
   \section{Debugging and tuning support}
*/

extern void pan_panic(const char *, ...);

/*
   Print the arguments of {\em pan\_panic} as in a printf call, then halt
   the panda program.
*/

extern void pan_sys_va_set_params(void *, ...);
extern void pan_sys_va_get_params(void *, ...);

/*
   Get/set parameters for system layer tuning/statistics.
   The interface is derived from X-windows XtVaSetValues/XtVaGetValues.
   Support of parameters is system dependent.
   Example:

   char *sys\_stats;\\
\\
   pan\_sys\_va\_get\_params(NULL, "PAN\_SYS\_statistics", \&sys\_stats, NULL);\\
   printf("%s", sys\_stats);\\
   free(sys\_stats);\\
*/

/*
   \section{Common Macros and Constants} 
*/


/*
typedef unsigned char byte_t, *byte_p;
*/

/* Add bitmasks with operations? */

#endif /* \_\_PANDA\_SYSTEM\_LAYER\_\_ */

#include "pan_sync.h"
#include "pan_timer.h"
