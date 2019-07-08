/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __SYS_MCAST_BITSET_H__
#define __SYS_MCAST_BITSET_H__

#define set_flag(field, flag)	((field) |= (flag))
#define unset_flag(field, flag)	((field) &= ~(flag))
#define isset_flag(field, flag)	((field) & (flag))

#endif
