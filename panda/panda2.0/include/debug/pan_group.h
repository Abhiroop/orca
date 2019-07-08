/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_DEBUG_GROUP_H__
#define __PANDA_DEBUG_GROUP_H__

void pan_group_va_set_params(void *dummy, ...);

void pan_group_va_get_params(void *dummy, ...);

/*
   \begin{flushleft}
   \verb%void pan_group_va_set_params([grp_resource_name, resource, ]*, NULL);%
   \verb%void pan_group_va_get_params([grp_resource_name, &resource, ]*, NULL);%
   \end{flushleft}

   Call these functions in the style of XtVaSetValues with pairs
   of arguments, the first denoting the name of a group resource, the second
   denoting a value of the type corresponding to that resource.
   The last argument must be NULL.

   These calls are used to set or get resource values that are shared between
   all groups.

   The defined resources are implementation-dependent.

   Examples of resources on amoeba:

   \begin{tabular}{l l}
     PAN\_GRP\_send\_timeout       & pan\_time\_p \\
     PAN\_GRP\_watch\_timeout      & pan\_time\_p \\
     PAN\_GRP\_bb\_boundary        & int \\
     PAN\_GRP\_hist\_buf\_size     & int \\
     PAN\_GRP\_ord\_buf\_size      & int \\
     PAN\_GRP\_sequencer           & int \\
   \end{tabular}

   Example calls:

   \begin{flushleft}
     \verb%int bb_bound;%
     \verb%pan_group_va_set_params(PAN_GRP_bb_boundary, 1250, NULL);%
     \verb%pan_group_va_get_params(PAN_GRP_bb_boundary, &bb_bound, NULL);%
   \end{flushleft}
*/


void pan_group_va_set_g_params(pan_group_p g, ...);
void pan_group_va_get_g_params(pan_group_p g, ...);

/*
   \begin{flushleft}
   \verb%void pan_group_va_set_g_params(pan_group_p g, [grp_resource_name, resource, ]*, NULL);%
   \verb%void pan_group_va_get_g_params(pan_group_p g, [grp_resource_name, &resource, ]*, NULL);%
   \end{flushleft}

   Call these functions in the style of XtVaSetValues with pairs of arguments,
   the first denoting the name of a group resource, the second denoting a
   value of the type corresponding to that resource.
   The last argument must be NULL.

   These calls are used to set or get resource values that are specific to
   one group.

   The defined resources are implementation-dependent.

   Examples of resources on amoeba:

   \begin{tabular}{l l}
     PAN\_GRP\_sequencer           & int \\
     PAN\_GRP\_statistics\_size    & int \\
     PAN\_GRP\_statistics          & char[] \\
     PAN\_GRP\_discards\_size      & int \\
     PAN\_GRP\_discards            & char[] \\
   \end{tabular}


   \begin{flushleft}
     \verb%void pan_group_va_set_params([grp_resource_name, resource, ]*, NULL);%
     \verb%void pan_group_va_get_params([grp_resource_name, &resource, ]*, NULL);%
   \end{flushleft}

   Call these functions in the style of XtVaSetValues with pairs of arguments,
   the first denoting the name of a group resource, the second denoting a
   value of the type corresponding to that resource. The last argument must
   be NULL.

*/

#endif /* \_\_PANDA\_DEBUG\_GROUP\_H\_\_ */
