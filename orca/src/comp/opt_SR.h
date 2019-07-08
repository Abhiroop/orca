/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __OPT_SR_H__
#define __OPT_SR_H__

/* S T R E N G T H   R E D U C T I O N */

/* $Id: opt_SR.h,v 1.7 1997/05/15 12:02:43 ceriel Exp $ */

#include	"ansi.h"
#include	"node.h"
#include	"def.h"

_PROTOTYPE(void do_SR, (p_def));
	/*	performs a combined strength-reduction/code-motion/
   		common-sub-expression-elimination.
   		See: "Lazy Strength Reduction" by Jens Knoop, Oliver Ruthing
		     and Bernard Steffen, Journal of Computer Languages Vol 1
		     No 1, pp 71-91, 1993.
	*/

_PROTOTYPE(int suitable_CMSR, (p_node nd));
        /*      Checks if the expression indicated by 'nd' is suitable
                for Code Motion and/or Strength Reduction.
                Returns -1 if it may not be moved at all,
                         0 if it may be moved but no gain is expected,
                         1 otherwise.
		The SR phase only moves expressions, so allocating temporaries
		too early could partly spoil the party. This routine can be
		used to detect those situations.
        */

#endif /* __OPT_SR_H__ */
