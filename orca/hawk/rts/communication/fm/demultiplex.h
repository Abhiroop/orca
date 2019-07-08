#ifndef __demultiplex__
#define __demultiplex__

#include "fm.h"
#include "pipe.h"

#define UPCALL -2

typedef struct protocol_s protocol_t, *protocol_p;

struct protocol_s {
  int pid;
  pipe_p pipe;
};

void receive_buffer(struct FM_buffer *buffer, int size);

int init_demultiplex(int moi, int gsize, int pdebug);
int finish_demultiplex();
protocol_p new_protocol();
int free_protocol(protocol_p);

#endif

