#ifndef __c_intf_h__
#define __c_intf_h__

#include <orca_types.h>

#define SO_REPLICATE -1
#define SO_DEFAULT   -2

#define SO_MAX_ARGS  1024

typedef t_object so_t;

typedef enum {SO_BLOCKED, SO_OK} so_status_t;

typedef so_status_t (* so_operation_t)(void *databuf, int dsize,
                                       void *argbuf, int asize,
                                       void *result, int rsize);

typedef void (*so_procfun_t)(void *arg, int argsize,
                             so_t **sh_args, int num_sh_args);

extern void so_main(int argc, char **argv);

int so_num_procs(void);

int so_my_proc(void);

void so_proc_create(int dest, so_procfun_t pf, void *arg, int size,
                    so_t **sh_args, int num_sh_args);

so_t *so_obj_create(int size, int cpu);

void so_read_only(so_t *obj, so_operation_t rd_op,
                  void *inbuf, int insize, void *outbuf, int outsize);

void so_pure_write(so_t *obj, so_operation_t wr_op, void *inbuf, int insize);

void so_read_write(so_t *obj, so_operation_t rw_op,
                   void *inbuf, int insize, void *outbuf, int outsize);


#endif /* __c_intf_h__ */

