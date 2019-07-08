#ifndef __RTS_OBJECT_H__
#define __RTS_OBJECT_H__

#include "orca_types.h"

#define RO_NO_OWNER   -1

extern void ro_init(rts_object_p ro, char *name);

extern void ro_clear(rts_object_p ro);

#endif
