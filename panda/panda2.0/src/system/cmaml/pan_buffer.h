#ifndef _SYS_CMAML_BUFFER_
#define _SYS_CMAML_BUFFER_


#include "pan_sys.h"
#include "pan_sys_pool.h"

#define UNIVERSAL_ALIGNMENT 8  /*
                                * maximum alignment of all types.
                                */
typedef struct buffer{
    POOL_HEAD;			/* Pool management */
    char   *b_data;
    int     b_index;
    int     b_size;
}pan_sys_buffer_t, *pan_sys_buffer_p;

void pan_sys_buffer_start(void);
void pan_sys_buffer_end(void);

pan_sys_buffer_p pan_sys_buffer_init(void) ;
void pan_sys_buffer_first(pan_sys_buffer_p buf, pan_fragment_p frag,
				 int preserve);
void  pan_sys_buffer_clear(pan_sys_buffer_p buf);
void *pan_sys_buffer_push(pan_sys_buffer_p buf, int size, int clear,
				 int align);
void *pan_sys_buffer_pop(pan_sys_buffer_p buf, int size, int align);
void *pan_sys_buffer_look(pan_sys_buffer_p buf, int size, int align);
void  pan_sys_buffer_copy(pan_sys_buffer_p buf, pan_sys_buffer_p copy);
void  pan_sys_buffer_resize(pan_sys_buffer_p buf, int size, int clear);


/* Internally, we can use macros */
#define pan_sys_buffer_empty(buf)   (buf)->b_index = 0
#define pan_sys_buffer_size(buf)    (buf)->b_index
#define pan_sys_buffer_data(buf)    (buf)->b_data
#define pan_sys_buffer_len(buf)     (buf)->b_size

#endif
