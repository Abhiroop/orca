/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: interface.h,v 1.39 1999/08/16 13:58:01 ceriel Exp $ */

#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "orca_types.h"

#ifdef NO_CHECKS
#define NO_ACHECK
#define NO_GCHECK
#define NO_UCHECK
#define NO_RCHECK
#define NO_FROMCHECK
#define NO_DIVCHECK
#define NO_CPUCHECK
#define NO_ASSERTIONS
#endif


/*	A R R A Y S */

void a_allocate(void *a, int ndim, size_t elsz, ...);
/* Allocates space for the array elements and fills in the entries in the
   t_array structure 'a'.  Space for possible dynamic compoments is not 
   allocated, because the size required is probably not known at this point.
   The array bounds are passed as integers. For every dimension, first a
   lb and then an ub is passed.
*/

#ifndef NO_ACHECK
#define a_check(a, i, dim, fn, ln) \
	if (((unsigned) ((i)-(a)->a_dims[dim].a_lwb)) >= (a)->a_dims[dim].a_nel) m_trap(ARRAY_BOUND_ERROR, fn, ln)
#define a_fixcheck(nels, i, fn, ln) \
	if ((unsigned)(i) >= (nels)) m_trap(ARRAY_BOUND_ERROR, fn, ln)
#else
#define a_check(a, i, dim, fn, ln)
#define a_fixcheck(nels, i, fn, ln)
#endif

/* memset should be in string.h, but is not on SunOS 4.1. */
void *memset(void *, int, size_t);

#define a_initialize(a, s) \
	((void) memset((void *) a, '\0', (size_t) sizeof(*a)))

#define a_size(a) \
	((a)->a_sz)

#define a_lb(a, i)	((a)->a_dims[i].a_lwb)

#define a_ne(a, i)	((a)->a_dims[i].a_nel)

#define a_ub(a, i)	((a)->a_dims[i].a_lwb+(a)->a_dims[i].a_nel-1)

/*	S E T S	*/

void s_addel(t_set *s, tp_dscr *d, void *el);
/* Add an element to set 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing INSERT.
*/
int s_delel(t_set *s, tp_dscr *d, void *el);
/* Remove an element from set 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing DELETE.
*/
t_boolean s_member(t_set *s, tp_dscr *d,  void *el);
/* Test membership of set 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing the IN operator.
*/
void s_from(t_set *s, tp_dscr *d, void *result);
/* Remove and store in 'result' a random element from set 's' of type 'd'.
*/

#define s_size(s)	((s)->s_nelem)

#define s_initialize(s, x) \
	((s)->s_nelem = 0, (s)->s_elem = 0)

#ifndef NO_FROMCHECK
#define s_fromcheck(s, fn, ln) \
	if ((s)->s_nelem == 0) m_trap(FROM_EMPTY_SET, fn, ln)
#else
#define s_fromcheck(s, fn, ln)
#endif

void s_add(t_set *a, t_set *b, tp_dscr *d);
/* Set union: a := a + b.
*/
void s_sub(t_set *a, t_set *b, tp_dscr *d);
/* Set difference: a := a - b.
*/
void s_inter(t_set *a, t_set *b, tp_dscr *d);
/* Set intersection: a := a * b.
*/
void s_symdiff(t_set *a, t_set *b, tp_dscr *d);
/* Symetric set difference: a := a / b.
*/

/* 	B A G S */

void b_addel(t_bag *s, tp_dscr *d, void *el);
/* Add an element to set 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing INSERT.
*/
int b_delel(t_bag *s, tp_dscr *d, void *el);
/* Remove an element from bag 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing DELETE.
*/
t_boolean b_member(t_bag *s, tp_dscr *d, void *el);
/* Test membership of bag 's' of type 'd'.
   The address of the element is passed in 'el'.
   This routine is used for implementing the IN operator.
*/
void b_from(t_bag *s, tp_dscr *d, void *result);
/* Remove and store in 'result' a random element from bag 's' of type 'd'.
*/

#define b_size(b)	((b)->s_nelem)

#define b_initialize(b, s) \
	((b)->s_nelem = 0, (b)->s_elem = 0)

#ifndef NO_FROMCHECK
#define b_fromcheck(s, fn, ln) \
	if ((s)->s_nelem == 0) m_trap(FROM_EMPTY_SET, fn, ln)
#else
#define b_fromcheck(s, fn, ln)
#endif

void b_add(t_bag *a, t_bag *b, tp_dscr *d);
/* Bag union: a := a + b.
*/
void b_sub(t_bag *a, t_bag *b, tp_dscr *d);
/* Bag difference: a := a - b.
*/
void b_inter(t_bag *a, t_bag *b, tp_dscr *d);
/* Bag intersection: a := a * b.
*/
void b_symdiff(t_bag *a, t_bag *b, tp_dscr *d);
/* Symmetric bag difference: a := a / b.
*/

/*	U N I O N S */

#ifndef NO_UCHECK
#define u_check(u, v, fn, ln) \
	if (! (u)->u_init || ((t_union *)((void *)(u)))->u_tagval != (v)) \
		m_trap(UNION_ERROR, fn, ln)
#else
#define u_check(u, v, fn, ln)
#endif

#define u_initialize(u, s) \
	((u)->u_init = 0)

/*	G R A P H S   A N D   N O D E N A M E S */

#ifndef NO_AGE
void g_addnode(t_nodename *res, void *g, size_t sz);
/* Adds a node to graph 'g', and stores the result in 'res'.
*/
void g_deletenode(t_nodename *n, void *g, void (*f)(void *));
/* Deletes the node indicated by 'n' from graph 'g'. A run-time error
   occurs if 'n' does not indicate a node of 'g'.
*/
#else
t_nodename g_addnode(void *g, size_t sz);
void g_deletenode(t_nodename n, void *g, void (*f)(void *));
#endif

void *g_getnode(void *g, size_t sz);

void g_freeblocks(void *g);

#ifdef NO_AGE
#define NO_GCHECK
#endif

#ifndef NO_GCHECK
#define g_check(g, n, fn, ln) \
	if ((g)->g_size <= (n)->n_index || \
	    (g)->g_mt[(n)->n_index].g_age != (n)->n_age) m_trap(BAD_NODENAME, fn, ln)
#else
#define g_check(g, n, fn, ln)
#endif

#define g_initialize(g, s) \
	((g)->g_size = 0, (g)->g_mt = 0, (g)->g_freelist = 0, (g)->g_freenodes = 0, (g)->g_ndlist = 0)

#ifndef NO_AGE
#define g_elem(g, n) \
	((g)->g_mt[(n)->n_index].g_node)

#define n_initialize(n, s) \
	((n)->n_age = 0, (n)->n_index = 0)

#define n_isnil(a) \
	((a)->n_age == 0)

#define cmp_nodename(a, b) \
	((a)->n_age == (b)->n_age && (a)->n_index == (b)->n_index)
#else
#define g_elem(g, n) \
	((g)->g_mt[(n)].g_node)

#define n_initialize(n, s) \
	(*(n) = 0)

#define n_isnil(a) \
	(*(a) == 0)

#define cmp_nodename(a, b) \
	(*(a) == *(b))
#endif

/*	M I S C E L L A N E O U S */

int m_ptrregister(void *f);
/* Register a pointer that may have to be marshalled. A unique integer
   that identifies the pointer is returned. This routine can be used to
   register a function address, a process descriptor or an operation
   descriptor.
*/

void *m_getptr(int i);
/* Returns the pointer that was stored earlier and given index 'i'.
   A run-time error occurs if 'm_ptrregister' never returned 'i'.
*/

void m_trap(int n, char *fn, int ln);
/* Causes run-time error number 'n' to occur.
*/

void m_aliaschk(void *a, void *b, tp_dscr *da, tp_dscr *db, char *fn, int ln);
/* Check that the data items indicated by a and b are no aliases.
   If they are, a run-time error occurs.
*/

void m_syserr(char *s);
/* For internal errors.
*/
 
void m_liberr(char *lib, char *s);
/* For library errors, f.i. I/O.
*/

#ifndef NO_DIVCHECK
#define m_divcheck(i, fn, nl)	if (i == 0) m_trap(DIV_ZERO, fn, nl)
#define m_modcheck(i, fn, nl)	if (i <= 0) m_trap(ILL_MOD, fn, nl)
#else
#define m_divcheck(i, fn, nl)
#define m_modcheck(i, fn, nl)
#endif
t_integer m_div(t_integer, t_integer);
t_longint m_divl(t_longint, t_longint);
t_integer m_mod(t_integer, t_integer);
t_longint m_modl(t_longint, t_longint);
/* Implement the integer division and modulo.
*/

#ifndef NO_ASSERTIONS
#define m_assert(b, fn, ln)	if (! b) m_trap(FAILED_ASSERT, fn, ln)
#else
#define m_assert(b, fn, ln)
#endif

#ifndef NO_CPUCHECK
#define m_cpucheck(n, fn, ln)	if (((unsigned) n >= m_ncpus())) m_trap(BAD_CPU, fn, ln)
#else
#define m_cpucheck(n, fn, ln)
#endif

#define m_cap(c) toupper(c)

#ifndef NO_RCHECK
t_integer m_check(t_integer, t_integer, char *, int);
#else
#define m_check(a, b, fn, ln)	(a)
#endif

t_integer m_abs(t_integer);
t_longint m_labs(t_longint);
t_real m_rabs(t_real);
t_longreal m_lrabs(t_longreal);

#ifndef PANDA4

/* Some stuff for marshalling: */

#define nget_tp(q, p, sz) \
	{ register char *__q = (char *) (q); \
	  switch(sz) { \
	  case 16: \
	    	*__q++ = *p++; \
	  case 15: \
		*__q++ = *p++; \
	  case 14: \
		*__q++ = *p++; \
	  case 13: \
		*__q++ = *p++; \
	  case 12: \
		*__q++ = *p++; \
	  case 11: \
		*__q++ = *p++; \
	  case 10: \
		*__q++ = *p++; \
	  case 9: \
	    	*__q++ = *p++; \
	  case 8: \
	    	*__q++ = *p++; \
	  case 7: \
	    	*__q++ = *p++; \
	  case 6: \
	    	*__q++ = *p++; \
	  case 5: \
	    	*__q++ = *p++; \
	  case 4: \
	    	*__q++ = *p++; \
	  case 3: \
		*__q++ = *p++; \
	  case 2: \
	    	*__q++ = *p++; \
	  case 1: \
	    	*__q++ = *p++; \
		break;\
	  } \
	}

#define get_tp(dst, p) \
	nget_tp(&(dst), p, sizeof(dst))

#define nput_tp(q, p, sz)	\
	{ register char *__q = (char *) (q); \
	  switch(sz) { \
	  case 16: \
	    	*p++ = *__q++; \
	  case 15: \
	    	*p++ = *__q++; \
	  case 14: \
	    	*p++ = *__q++; \
	  case 13: \
	    	*p++ = *__q++; \
	  case 12: \
	    	*p++ = *__q++; \
	  case 11: \
	    	*p++ = *__q++; \
	  case 10: \
	    	*p++ = *__q++; \
	  case 9: \
	    	*p++ = *__q++; \
	  case 8: \
	    	*p++ = *__q++; \
	  case 7: \
	    	*p++ = *__q++; \
	  case 6: \
	    	*p++ = *__q++; \
	  case 5: \
	    	*p++ = *__q++; \
	  case 4: \
	    	*p++ = *__q++; \
	  case 3: \
	    	*p++ = *__q++; \
	  case 2: \
	    	*p++ = *__q++; \
	  case 1: \
	    	*p++ = *__q++; \
		break; \
	  } \
	}

#define put_tp(src, p)	\
	nput_tp(&(src), p, sizeof(src))

/* Marshalling functions for the string type: */
int sz_string(t_string *);
char *ma_string(char *, t_string *);
char *um_string(char *, t_string *);

#else
int sz_string(t_string *);
pan_iovec_p ma_string(pan_iovec_p , t_string *);
void um_string(void *, t_string *);
#endif

/* Type-specific functions for the built-in Orca types: */
void free_string(void *s);
void ass_string(void *, void *);
int cmp_string(void *, void *);

int (cmp_enum)(void *, void *);
int (cmp_longenum)(void *, void *);
int (cmp_integer)(void *, void *);
int (cmp_longint)(void *, void *);
int (cmp_shortint)(void *, void *);
int (cmp_real)(void *, void *);
int (cmp_longreal)(void *, void *);
int (cmp_shortreal)(void *, void *);
int (cmp_nodename)(void *, void *);
void ass_enum(void *, void *);
void ass_longenum(void *, void *);
void ass_integer(void *, void *);
void ass_longint(void *, void *);
void ass_shortint(void *, void *);
void ass_real(void *, void *);
void ass_longreal(void *, void *);
void ass_shortreal(void *, void *);
void ass_nodename(void *, void *);

/****************************************************************/
/* Start of system-dependent part:				*/
/****************************************************************/

#ifndef m_ncpus
t_integer m_ncpus(void);
/* Returns the number of CPUs.
*/
#endif

#ifndef m_mycpu
t_integer m_mycpu(void);
/* Returns the CPU number of the calling process.
*/
#endif

#ifndef m_objdescr_reg
void m_objdescr_reg(tp_dscr *d, int n_oper, char *n);
/* Register an object type with name 'n' to the runtime system (currently used
   for tracing).
*/
#endif

#ifndef o_start_read
t_boolean o_start_read(t_object *o);
/* Test if a read-operation on object 'o' may be done locally (and give run-time
   system opportunity to set a read-lock).
*/
#endif

#ifndef o_end_read
void o_end_read(t_object *o);
/* Indicate to the run-time system that a read-operation has been done locally,
   (after a succesful call to o_start_read), and that the read-lock may be
   released.
*/
#endif

#ifndef o_start_write
t_boolean o_start_write(t_object *o);
/* Test if a write-operation on object 'o' may be done locally (and give
   run-time system opportunity to set a write-lock).
*/
#endif

#ifndef o_end_write
void o_end_write(t_object *o, int result);
/* Indicate to the run-time system that a write-operation has been attempted
   (after a succesful call to o_start_write). If 'result' is set, the
   object has indeed been written. The write-lock is released.
*/
#endif

#ifndef o_isshared
int o_isshared(t_object *o);
/* Returns non-zero if the indicated object is shared (between multiple Orca
   processes).
*/
#endif

#ifndef o_free
int o_free(t_object *o);
/* Free RTS part of the object 'o'. Return 1 if data part must be freed.
*/
#endif

#ifndef o_init_rtsdep
void o_init_rtsdep(t_object *o, tp_dscr *d, char *n);
#endif
/* Initialize the system-dependent part of an object.
*/

#ifdef DATA_PARALLEL
void p_addscr(po_p p, int opid, tp_dscr *d, int opno);
void *p_gatherinit_f(instance_p ip, void *a, void *d);

void p_partition(instance_p ip, ...);
void p_distribute(instance_p i, po_opcode init, void *a);
void p_distribute_on_n(instance_p i, po_opcode init, ...);
void p_distribute_on_list(instance_p i, po_opcode init, void *cpulist, ...);

#define p_clear_dependencies(i, o)	do_clear_dependencies(i, o)
#define p_set_dependencies(i, o)	do_set_dependencies(i, o)
#define p_add_dependency		do_add_dependency
#define p_remove_dependency		do_remove_dependency
#endif

/*	P R O C E S S E S */

void DoFork(int cpu, prc_dscr *procdscr, void **argtab);
/* Fork the process indicated by 'procdscr' on processor 'cpu'.
*/

#define READS	1
#define WRITES	2

#define BLOCKING 1
#define NESTED	 2

void DoOperation(t_object *o, int *op_flags, tp_dscr *d, int opindex,
		 int attempted, void **argtab);
/* Handles the general case of an operation.
   The object on which the operation is to be performed is in 'o',
   the object descriptor is in 'd',
   on entry, 'op_flags' indicates whether the operation is nested, on return
   it indicates if the operation blocks,
   the operation descriptor is indicated by 'opindex',
   'attempted' indicates what has already been tried (WRITES, READS,
   READS|WRITES, or 0),
   and the operation parameters are indicated by argtab.
*/

#ifndef m_malloc
void	*m_malloc(size_t);
/* Storage allocator. Like malloc(), but traps if memory could not be allocated.
*/
#endif

#ifndef m_realloc
void	*m_realloc(void *p, size_t);
/* Storage re-allocator. Like realloc(), but traps if memory could not be
   allocated.
*/
#endif

#ifndef m_free
void	m_free(void *);
/* Frees previously allocated memory.
*/
#endif

#ifndef m_rts
void m_rts(void);
/* Polling hook. On systems with pre-emption, m_rts should be defined as an
   empty macro.
*/
#endif

#ifndef m_strategy
void m_strategy(t_object *o, int replicated, int owner);
/* Implements strategy calls. If 'replicated' is non-zero, replicate the object.
   Otherwise, store the object on the CPU with number 'owner'.
*/
#endif

/* Score functions, called by compiler-generated code at start and end of a
   process, for objects or arrays of objects.
*/
#ifndef __Score
void __Score(void *data, tp_dscr *d,
	     double score, double naccess, double uncertainty);
#endif

#ifndef __erocS
void __erocS(void *data, tp_dscr *d,
	     double score, double naccess, double uncertainty);
#endif
   
#ifndef o_rts_nbytes
extern int o_rts_nbytes(t_object *op, tp_dscr *d);
#endif

#ifndef PANDA4

#ifndef o_rts_marshall
extern char *o_rts_marshall(char *p, t_object *op, tp_dscr *d);
#endif

#ifndef o_rts_unmarshall
extern char *o_rts_unmarshall(char *p, t_object *op, tp_dscr *d);
#endif

#else

#ifndef o_rts_marshall
extern pan_iovec_p o_rts_marshall(pan_iovec_p p, t_object *op, tp_dscr *d);
#endif

#ifndef o_rts_unmarshall
extern void o_rts_unmarshall(void *p, t_object *op, tp_dscr *d);
#endif

#endif

#ifndef m_print
void m_print(FILE *f, char *s, int len);
#endif

#ifndef m_scan
int m_scan(FILE *f, char *s, void *p);
#endif

#ifndef m_flush
int m_flush(FILE *f);
#endif

#ifdef DATA_PARALLEL
/* Interface for Saniya's RTS: */

void opmarshall_f(message_p mp, void **arg, void *d);
char * opunmarshall_f(char *p, void ***arg, void *d, void **args, int sender, instance_p ip);
void opfree_in_params_f(void **args, void *d, int remove_outparams);
#endif
