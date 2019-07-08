/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_BUFFER_
#define _SYS_GENERIC_BUFFER_

#include <stddef.h>

#include <fm.h>

#include "pan_sys.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#define PACKET_SIZE		((0x1) << LOG_PACKET_SIZE)


typedef enum BUFFER_TYPE {
    BUF_NORMAL		= 0,
    BUF_FM_BUFFER	= (0x1) << 0
} buffer_type_t;


typedef union PAN_SYS_BUFFER {
    struct {
	buffer_type_t	b_type;
	int		b_index;
	int		b_size;
    }		     b;
    struct FM_buffer f;
} pan_sys_buffer_t, *pan_sys_buffer_p;


#define BUFFER_DESCR_SIZE	offsetof(struct FM_buffer, fm_buf)

#define fm2buffer(fm)	((pan_sys_buffer_p)(fm))
#define buffer2fm(buf)	(&(buf)->f)



#ifndef STATIC_CI

extern void pan_sys_buffer_start(void);
extern void pan_sys_buffer_end(void);

extern pan_sys_buffer_p pan_sys_buffer_create(void) ;
extern void  pan_sys_buffer_clear(pan_sys_buffer_p buf);
extern void *pan_sys_buffer_push(pan_sys_buffer_p *p_buf, int size);
extern void *pan_sys_buffer_pop(pan_sys_buffer_p buf, int size, int align);
extern void *pan_sys_buffer_look(pan_sys_buffer_p buf, int size, int align);
extern void  pan_sys_buffer_copy(pan_sys_buffer_p buf, pan_sys_buffer_p copy);
extern void  pan_sys_buffer_append(pan_sys_buffer_p buf, pan_sys_buffer_p next,
				   int offset, int size);

#endif


/* In this module, use macros to access the struct fields */
#define pan_sys_buffer_empty(buf)	((buf)->b.b_index = 0)

#define buffer_type(buf)		((buf)->b.b_type)
#define buffer_offset(buf)		((buf)->b.b_index)
#define buffer_data(buf)		((buf)->f.fm_buf)
#define buffer_size(buf)		((buf)->b.b_size)

#endif
