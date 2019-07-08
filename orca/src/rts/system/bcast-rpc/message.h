/* $Id: message.h,v 1.3 1994/03/11 10:51:45 ceriel Exp $ */

struct ObjectEntry {
    int		obj_id;
    t_object	obj_descr;
    struct ObjectEntry *obj_next;
};

#define MAXENTRIES	512

#ifndef CHK_FORK_MSGS
#define doexit(n)	exit(n)
#endif
