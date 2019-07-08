#ifndef __PAN_GRP_BB_ACPT_H__
#define __PAN_GRP_BB_ACPT_H__


#include "pan_sys.h"



#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

pan_fragment_p bb_acpt_get(void);
void           bb_acpt_put(pan_fragment_p frag);
void           bb_acpt_start(void);
void           bb_acpt_end(void);

#endif


#endif
