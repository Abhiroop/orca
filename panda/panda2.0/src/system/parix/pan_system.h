#ifndef _SYS_GENERIC_SYSTEM_
#define _SYS_GENERIC_SYSTEM_

#include "pan_parix.h"

#include <pan_sys.h>


typedef enum PROTOCOL_BITS {
    PROTO_normal	= 0,
    PROTO_link_conn	= (0x1 << 0),
    PROTO_dedicated	= (0x1 << 1),
    PROTO_pure_GSB	= (0x1 << 2),
    PROTO_pipe_ucast	= (0x1 << 3),
    PROTO_indirect_pb	= (0x1 << 4)
} protocol_bits_t, *protocol_bits_p;


extern int  pan_sys_pid;			/* Excluding dedicated seq. */
extern int  pan_sys_nr;				/* Excluding dedicated seq. */
extern int  pan_sys_total_platforms;		/* Including dedicated seq. */
extern int  pan_sys_Parix_id;			/* Including dedicated seq. */
extern int  pan_sys_DimX;
extern int  pan_sys_DimY;
extern int  pan_sys_protocol;
extern int  pan_sys_sequencer;
extern int  pan_sys_verbose_level;

extern int  pan_sys_started;

extern int  pan_sys_dedicated;
extern int  pan_sys_link_connected;
extern int  pan_sys_save_check;
extern int  pan_sys_verbose_level;

extern int  pan_sys_mem_startup;

int  pan_sys_proc_mapping(int pid);

#define pan_my_pid()		pan_sys_pid
#define pan_nr_platforms()	pan_sys_nr

#endif
