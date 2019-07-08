/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_SYSTEM_
#define _SYS_GENERIC_SYSTEM_

extern int pan_sys_nr;
extern int pan_sys_pid;
extern int pan_sys_started;
extern int pan_comm_verbose;

#define pan_my_pid()		pan_sys_pid
#define pan_nr_platforms()	pan_sys_nr

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif
