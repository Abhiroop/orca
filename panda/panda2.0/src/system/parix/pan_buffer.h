#ifndef _SYS_GENERIC_BUFFER_
#define _SYS_GENERIC_BUFFER_


#include "pan_sys.h"
#include "pan_sys_pool.h"

#define aligned(p, align)   ((((long)(p)) & ((align) - 1)) == 0)
#define do_align(p, align) \
    ((p + (align) - 1) & ~((align) - 1))

#define UNIVERSAL_ALIGNMENT 4  /*
                                * maximum alignment of all types.
                                */
#define univ_align(p)	do_align(p, UNIVERSAL_ALIGNMENT)

typedef struct buffer{
    POOL_HEAD;			/* Pool management */
    char   *b_data;
    int     b_index;
    int     b_size;
}pan_sys_buffer_t, *pan_sys_buffer_p;

extern void pan_sys_buffer_start(void);
extern void pan_sys_buffer_end(void);

extern pan_sys_buffer_p pan_sys_buffer_init(void) ;
extern void pan_sys_buffer_first(pan_sys_buffer_p buf, pan_fragment_p frag,
				 int preserve);
extern void  pan_sys_buffer_clear(pan_sys_buffer_p buf);
extern void *pan_sys_buffer_push(pan_sys_buffer_p buf, int size, int clear,
				 int align);
extern void *pan_sys_buffer_pop(pan_sys_buffer_p buf, int size, int align);
extern void *pan_sys_buffer_look(pan_sys_buffer_p buf, int size, int align);
extern void  pan_sys_buffer_copy(pan_sys_buffer_p buf, pan_sys_buffer_p copy);
extern void  pan_sys_buffer_resize(pan_sys_buffer_p buf, int size, int clear);
extern void  pan_sys_buffer_append(pan_sys_buffer_p buf, pan_fragment_p frag);
extern char *pan_sys_buffer_offset(pan_sys_buffer_p buf, int offset,
				   int *size, int clear, int *last);


/* Internally, we can use macros */
#define pan_sys_buffer_empty(buf)   (buf)->b_index = 0
#define pan_sys_buffer_size(buf)    (buf)->b_index
#define pan_sys_buffer_data(buf)    (buf)->b_data
#define pan_sys_buffer_len(buf)     (buf)->b_size

#endif
