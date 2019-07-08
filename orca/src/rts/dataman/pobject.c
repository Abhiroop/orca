/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: pobject.c,v 1.4 1998/05/13 11:47:29 ceriel Exp $ */

#ifdef DATA_PARALLEL

#include <stdio.h>
#include <interface.h>
#include <rts_internals.h>

/* Partitioned objects stuff ... */

void
p_adddscr(po_p p, int opid, tp_dscr *d, int opno)
{
  po_operation_p opp = get_item(p->op_table, opid);
  opp->opdescr = &(td_operations(d)[opno]);
}

void *
p_gatherinit_f(instance_p ip, void *ap, void *dp)
{
  t_array
	*a = ap;
  tp_dscr
	*d = dp;
  int	i;
  int	offset = 0;
  int	sz = 1;
  void	*p;

  /* For now: only for static-sized types. */
  if (a->a_sz > 0) {
  	p = (char *)(a->a_data) + td_elemdscr(d)->td_size*a->a_offset;
	m_free(p);
  }

  for (i = 0; i < td_ndim(d); i++) {
	int	lb, ub;

	lb = ip->state->start[i];
	ub = ip->state->end[i];
	a->a_dims[i].a_lwb = lb;
	if (ub < lb) {
		sz = 0;
		ub = lb - 1;
	}
	a->a_dims[i].a_nel = ub - lb + 1;
	if (i == 0) {
		sz = (ub - lb + 1);
	}
	else {
		sz *= (ub - lb + 1);
		offset *= (ub - lb + 1);
	}
	offset += lb;
  }

  d = td_elemdscr(d);
  if (sz <= 0) {
	a->a_data = 0;
	a->a_sz = 0;
	p = 0;
  }
  else {
  	a->a_sz = sz;
  	a->a_offset = offset;
        p = m_malloc(a->a_sz * d->td_size);
  	if (offset == 0) a->a_data = (char *) p;
  	else a->a_data = (char *) p - d->td_size*offset;
  }
  return (char *) p;
}

void
p_partition(instance_p ip, ...)
{
  /* For each dimension, the user gives the number of partitions along
     that dimension.
  */
  va_list
	ap;
  int	*pd = m_malloc(sizeof(int) * ip->po->num_dims);
  int	i;

  va_start(ap, ip);
  for (i = 0; i < ip->po->num_dims; i++) {
	pd[i] = va_arg(ap, int);
  }
  va_end(ap);
  do_my_partitioning(ip, pd);
  m_free(pd);
}

void
p_distribute(instance_p ip, po_opcode init, void *ap)
{
  t_array *a = ap;
  do_my_distribution(ip, &((int *) (a->a_data))[a->a_offset]);
  do_operation(ip, init, (void **) 0);
  wait_for_end_of_invocation(ip);
}

void
p_distribute_on_n(instance_p ip, po_opcode init, ...)
{
  va_list
	ap;
  int	*dist[2];
  int	i;

  dist[0] = m_malloc(sizeof(int) * ip->po->num_dims);
  dist[1] = m_malloc(sizeof(int) * ip->po->num_dims);
  va_start(ap, init);
  for (i = 0; i < ip->po->num_dims; i++) {
	dist[0][i] = va_arg(ap, int);
	dist[1][i] = va_arg(ap, int);
  }
  va_end(ap);
  do_distribute_on_n(ip, dist);
  m_free(dist[0]);
  m_free(dist[1]);
  do_operation(ip, init, (void **) 0);
  wait_for_end_of_invocation(ip);
}

void
p_distribute_on_list(instance_p ip, po_opcode init, void *a, ...)
{
  va_list
	ap;
  int	*dist[2];
  int	i;
  t_array
	*cpulist = a;

  dist[0] = m_malloc(sizeof(int) * ip->po->num_dims);
  dist[1] = m_malloc(sizeof(int) * ip->po->num_dims);
  va_start(ap, a);
  for (i = 0; i < ip->po->num_dims; i++) {
	dist[0][i] = va_arg(ap, int);
	dist[1][i] = va_arg(ap, int);
  }
  va_end(ap);
  do_distribute_on_list(ip,
			cpulist->a_sz,
			&((int *) (cpulist->a_data))[cpulist->a_offset],
			dist);
  m_free(dist[0]);
  m_free(dist[1]);
  do_operation(ip, init, (void **) 0);
  wait_for_end_of_invocation(ip);
}

#endif /* DATA_PARALLEL */
