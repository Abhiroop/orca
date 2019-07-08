/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_GROUP_H__
#define __PANDA_GROUP_H__

/*
   The group module provides reliable, totally ordered group
   communication with dynamic, closed groups.

   This module depends upon:
   \begin{itemize}
   \item the system module
   \end{itemize}
*/

#include "pan_sys.h"

/*
   Opaque type for the group data structure
*/


typedef struct PAN_GROUP_T pan_group_t, *pan_group_p;


void pan_group_init(void);

void pan_group_end(void);

pan_group_p pan_group_join(char *group_name,
			   void (*receive)(pan_msg_p msg_in));
/*
   Joins a group with name {\em name}.  If no such group exists, a new
   group will be created.  Messages sent to this group will be handled
   through subsequent calls of {\em receive}; message handling is
   single-threaded and totally ordered. The user is responsible for
   clearing the upcall argument {\em msg\_in}.
*/

void pan_group_leave(pan_group_p g);
/*
   Leaves {\em group}.
*/

void pan_group_clear(pan_group_p g);
/*
   Clear the group state. Between the calls to {\em pan\_group\_leave}
   and {\em pan\_group\_clear}, the group state can be accessed for
   debugging information.
*/

void pan_group_send(pan_group_p g, pan_msg_p msg);
/*
   Sends {\em message} to group {\em group}. Messages sent by
   pan\_group\_send will be delivered in total order to all members.
*/

void pan_group_await_size(pan_group_p g, int n);
/*
   Waits until group {\em group} consists of {\em size} members.
*/


extern void pan_group_va_set_params(void *, ...);
extern void pan_group_va_get_params(void *, ...);

/*
   Set/get parameters shared between all groups
*/

extern void pan_group_va_set_g_params(pan_group_p, ...);
extern void pan_group_va_get_g_params(pan_group_p, ...);

/*
    Set/get parameters specific to one group
*/

#endif /* \_\_PANDA\_GROUP\_H\_\_ */
