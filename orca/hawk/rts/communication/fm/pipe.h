#ifndef __pipe__
#define __pipe__

#include "amoeba.h"
#include "fm.h"

typedef struct pipe_s pipe_t, *pipe_p;

int init_pipe(int moi, int gsize, int pdebug);
int finish_pipe();

pipe_p new_pipe();
int free_pipe();

int pipe_append(pipe_p, struct FM_buffer *buffer);
struct FM_buffer *pipe_retrieve(pipe_p);

#endif
