#ifndef __sys_mp_channel__
#define __sys_mp_channel__

#include "set.h"
#include "amoeba.h"
#include "fm.h"
#include "demultiplex.h"
#include "mp_channel.h"

struct mp_channel_s {
  int ch_number;
  set_p group;
  FM_mc_t mid;
  protocol_p protocol;
};

#endif
