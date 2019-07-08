#include <interface.h>
#include <InOut.h>
#include <IntObject.h>
#include <args.h>

#include "c_intf.h"

static void so_startoff(void **argv);

static int sz_OrcaMain_args(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_OrcaMain_args(pan_iovec_p p, void **argv);
static void um_OrcaMain_args(void *p, void **ap);
#else
static char *ma_OrcaMain_args(char *p, void **argv);
static char *um_OrcaMain_args(char *p, void **ap);
#endif

static par_dscr so_OrcaMain_params[] = {
    { 0, 0, 0, 0}
};

static tp_dscr so_OrcaMain_desc = {
    FUNCTION,                         /* this is a function descriptor */
    sizeof(t_integer),                /* size of the registration id */
    0,                                /* no flags */
    0,                                /* no result type */
    0,                                /* zero parameters */
    so_OrcaMain_params                /* parameter descriptors */
};

static sh_args zero_stats[SO_MAX_ARGS];

prc_dscr OrcaMain = {
    so_startoff,                      /* the process function */
    &so_OrcaMain_desc,                /* process function descriptor */
    0,                                /* registration number */
    zero_stats,                       /* argument statistics */
    "so_OrcaMain",                    /* name */
    sz_OrcaMain_args, 
    ma_OrcaMain_args,
    um_OrcaMain_args
};

static void
so_startoff(void **argv)
{
    t_string tmp;
    int so_argc, i;
    char **so_argv;

    so_argc = (int)f_args__Argc();
    so_argv = m_malloc((so_argc + 1) * sizeof(void *));
    
    a_initialize(&tmp, "so_tmp");
    for (i = 0; i < so_argc; i++) {
        f_args__Argv((t_integer)i, &tmp);
        so_argv[i] = m_malloc(tmp.a_sz + 1);
        so_argv[i][tmp.a_sz] = '\0';
        strncpy(so_argv[i], &((char *)(tmp.a_data))[tmp.a_offset], tmp.a_sz);
    }
    free_string(&tmp);
    so_argv[so_argc] = 0;

    so_main(so_argc, so_argv);

    for (i = 0; i < so_argc; i++) {
        m_free(so_argv[i]);
    }
    m_free(so_argv);
}


static void so_proc_declare(void);
static void so_obj_declare(void);

void ini_OrcaMain(void)
{
    static int done = 0;
    
    if (done) {
        return;
    }
    done = 1;

    OrcaMain.prc_registration = m_ptrregister((void *)&OrcaMain);

    so_proc_declare();
    so_obj_declare();

    ini_args__args();
    ini_InOut__InOut();
}

static int 
sz_OrcaMain_args(void **argv)
{
    return 0;
}

#ifdef PANDA4
static pan_iovec_p
ma_OrcaMain_args(pan_iovec_p p, void **argv)
{
    return p;
}

static void
um_OrcaMain_args(void *p, void **ap)
{
}
#else
static char *
ma_OrcaMain_args(char *p, void **argv)
{
    return p;
}

static char *
um_OrcaMain_args(char *p, void **ap)
{
    return p;
}
#endif

int
so_num_procs(void)
{
    return m_ncpus();
}

int
so_my_proc(void)
{
    return m_mycpu();
}

static fld_dscr field_desc[1] = {
    {0, &td_string}                     /* just one field of type array */
};

static tp_dscr record_desc = {
    RECORD,
    sizeof(t_array),
    DYNAMIC|INIT_CODE,
    0,                                   /* not used */
    1,                                   /* just one field */
    &field_desc                          /* field descriptor */
};

static int sz_object(t_object *op);
#ifdef PANDA4
static pan_iovec_p ma_object(pan_iovec_p p, t_object *op);
static void um_object(void *p, t_object *op);
#else
static char *ma_object(char *p, t_object *op);
static char *um_object(char *p, t_object *op);
#endif

extern op_dscr operation_desc[];       /* forward declaration */

static obj_info obj_info_desc = {
    sz_object, ma_object, um_object,   /* object marshalling functions */
    free_string,		       /* free-ing function of object record */
    operation_desc                     /* object operation descriptors */
};

static tp_dscr obj_desc = {
    OBJECT,
    sizeof(t_object), 
    DYNAMIC|NO_EQ|INIT_CODE|HAS_OBJECTS,
    &record_desc,                       /* object state descriptor */
    0,                                  /* registration id */
    &obj_info_desc                      /* marshalling and operations */
};

static void so_obj_declare(void)
{
    obj_desc.td_misc2 = m_ptrregister((void *) &obj_desc);
}

so_t *
so_obj_create(int size, int cpu)
{
    t_object *obj;

    if (cpu != SO_REPLICATE && cpu != SO_DEFAULT &&
        (cpu < 0 || cpu >= m_ncpus())) {
        fprintf(stderr, "so_obj_create: illegal cpu number (%d)\n", cpu);
        exit(1);
    }

    obj = m_malloc(sizeof(t_object));

    obj->o_fields = m_malloc(sizeof(t_string));
    a_allocate(obj->o_fields, 1, 1, 0, size - 1);
    o_init_rtsdep(obj, &obj_desc, "so object");
    
    __Score(obj, &obj_desc, 0.0, 0.0, 0.0);

    if (cpu != SO_DEFAULT) {
        m_strategy(obj, (cpu == SO_REPLICATE), cpu);
    }

    return (so_t *)obj;
}

static int
sz_object(t_object *op)
{
	return sz_string(op->o_fields);
}

#ifdef PANDA4
static pan_iovec_p 
ma_object(pan_iovec_p p, t_object *op)
{
	return ma_string(p, op->o_fields);
}

static void
um_object(void *p, t_object *op)
{
        if (! op->o_fields) {
            op->o_fields = m_malloc(sizeof(t_object));
        }
        um_string(p, op->o_fields);
}
#else
static char *
ma_object(char *p, t_object *op)
{
	return ma_string(p, op->o_fields);
}

static char *
um_object(char *p, t_object *op)
{
        if (! op->o_fields) {
            op->o_fields = m_malloc(sizeof(t_object));
        }
        return um_string(p, op->o_fields);
}
#endif

static int proc_arg_size(void **argv);
#ifdef PANDA4
static pan_iovec_p proc_arg_marshall(pan_iovec_p p, void **argv);
static void proc_arg_unmarshall(void *pp, void **argv);
#else
static char *proc_arg_marshall(char *p, void **argv);
static char *proc_arg_unmarshall(char *pp, void **argv);
#endif

static void handle_fork(void **argv);

static prc_dscr so_procs[SO_MAX_ARGS + 1];
static tp_dscr so_funcs[SO_MAX_ARGS + 1];

static void
so_proc_declare(void)
{
    par_dscr *params;
    int i;

    params = m_malloc((3 + SO_MAX_ARGS) * sizeof(par_dscr));

    for (i = 0; i <= SO_MAX_ARGS; i++) {

        so_procs[i].prc_name = handle_fork;
        so_procs[i].prc_func = &so_funcs[i];
        so_procs[i].prc_registration = m_ptrregister((void *)&so_procs[i]);
        so_procs[i].prc_shargs = zero_stats;
        so_procs[i].prc_trc_name = "";  /* nonnull for pan_thread_create */
        so_procs[i].prc_size_args       = proc_arg_size;
        so_procs[i].prc_marshall_args   = proc_arg_marshall;
        so_procs[i].prc_unmarshall_args = proc_arg_unmarshall;

        so_funcs[i].td_type  = FUNCTION;
        so_funcs[i].td_size  = sizeof(t_integer);
        so_funcs[i].td_flags = 0;
        so_funcs[i].td_misc1 = 0;             /* no return value */
        so_funcs[i].td_misc2 = i + 3;         /* #parameters */
        so_funcs[i].td_misc3 = params;
    }
    
    /* parameter descriptors */
    params[0].par_mode = IN;
    params[0].par_descr = &td_integer;
    params[0].par_copy = ass_integer;
    params[0].par_free = 0;

    params[1].par_mode = IN;
    params[1].par_descr = &td_integer;
    params[1].par_copy = ass_integer;
    params[1].par_free = 0;

    params[2].par_mode = IN;
    params[2].par_descr = &td_string;
    params[2].par_copy = ass_string;
    params[2].par_free = free_string;

    for (i = 0; i < SO_MAX_ARGS; i++) {
        params[i + 3].par_mode = SHARED;
        params[i + 3].par_descr = &obj_desc;
        params[i + 3].par_copy = 0;  /* ??? */
        params[i + 3].par_free = 0;  /* ??? */
    }
}

void
so_proc_create(int dest, so_procfun_t pf, void *arg, int size,
               so_t **sh_args, int num_sh_args)
{
    int num_args = num_sh_args + 3;
    void **argv;
    t_array buf;
    int i;

    buf.a_data = arg;
    buf.a_offset = 0;
    buf.a_sz = size;
    buf.a_dims[0].a_lwb = 0;
    buf.a_dims[0].a_nel = size;
 
    argv = m_malloc(num_args * sizeof(void *));
    argv[0] = &pf;
    argv[1] = &num_sh_args;
    argv[2] = &buf;
    for (i = 0; i < num_sh_args; i++) {
        argv[i + 3] = sh_args[i];
    }

    DoFork(dest, &so_procs[num_sh_args], argv);

    m_free(argv);
}

static void
handle_fork(void **argv)
{
    int num_sh_args, i;
    so_procfun_t procfun;
    t_array *inbuf;
    so_t **sh_args;

    procfun = *(so_procfun_t *)(argv[0]);
    num_sh_args = *(int *)(argv[1]);
    inbuf = (t_array *)(argv[2]);

    sh_args = m_malloc(num_sh_args * sizeof(so_t *));
    for (i = 0; i < num_sh_args; i++) {
        sh_args[i] = argv[i + 3];
    }

    (*procfun)(inbuf->a_data, inbuf->a_sz, sh_args, num_sh_args);

    m_free(sh_args);
}

#ifdef PANDA4
static int
proc_arg_size(void **argv)
{
    return 2 + sz_string(argv[2]);
}

static pan_iovec_p
proc_arg_marshall(pan_iovec_p p, void **argv)
{
    p->len = sizeof(int);
    p->data = argv[0];
    p++;
    p->len = sizeof(int);
    p->data = argv[1];
    p++;

    return ma_string(p, argv[2]);
}

static void
proc_arg_unmarshall(void *p, void **argv)
{
    int procfunc, num_shared;

    pan_msg_consume(p, (void *) &procfunc, sizeof(int));
    pan_msg_consume(p, (void *) &num_shared, sizeof(int));
    argv[0] = m_malloc(sizeof(int));
    *(int *)(argv[0]) = procfunc;

    argv[1] = m_malloc(sizeof(int));
    *(int *)(argv[1]) = num_shared;

    argv[2] = m_malloc(sizeof(t_array));
    um_string(p, argv[2]);
}
#else
static int
proc_arg_size(void **argv)
{
    int sz = 2 * sizeof(t_integer) + sz_string(argv[2]);
    return sz;
}

static char *
proc_arg_marshall(char *p, void **argv)
{
    (void)memcpy(p, argv[0], sizeof(int));
    p += sizeof(int);

    (void)memcpy(p, argv[1], sizeof(int));
    p += sizeof(int);

    p = ma_string(p, argv[2]);

    return p;
}

static char *
proc_arg_unmarshall(char *p, void **argv)
{
    int procfunc, num_shared;

    (void)memcpy(&procfunc, p, sizeof(int));
    p += sizeof(int);
    
    (void)memcpy(&num_shared, p, sizeof(int));
    p += sizeof(int);
    
    argv[0] = m_malloc(sizeof(int));
    *(int *)(argv[0]) = procfunc;

    argv[1] = m_malloc(sizeof(int));
    *(int *)(argv[1]) = num_shared;

    argv[2] = m_malloc(sizeof(t_array));
    p = um_string(p, argv[2]);

    return p;
}
#endif

static int sz_op_call(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_op_call(pan_iovec_p p, void **argv);
static void um_op_call(void *p, void ***ap);
#else
static char *ma_op_call(char *p, void **argv);
static char *um_op_call(char *p, void ***ap);
#endif
static int sz_retval(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_retval(pan_iovec_p p, void **argv);
static void um_retval(void *p, void **argv);
#else
static char *ma_retval(char *p, void **argv);
static char *um_retval(char *p, void **argv);
#endif
static void fr_retval(void **argv);

static int sz_read_call(void **argv) ;
#ifdef PANDA4
static pan_iovec_p ma_read_call(pan_iovec_p p, void **argv);
static void um_read_call(void *p, void ***ap);
#else
static char *ma_read_call(char *p, void **argv);
static char *um_read_call(char *p, void ***ap);
#endif
static int sz_read_ret(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_read_ret(pan_iovec_p p, void **argv);
static void um_read_ret(void *p, void **argv);
#else
static char *ma_read_ret(char *p, void **argv);
static char *um_read_ret(char *p, void **argv);
#endif
static void fr_read_ret(void **argv);

static int sz_write_call(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_write_call(pan_iovec_p p, void **argv);
static void um_write_call(void *p, void ***ap);
#else
static char *ma_write_call(char *p, void **argv);
static char *um_write_call(char *p, void ***ap);
#endif
static int sz_write_ret(void **argv);
#ifdef PANDA4
static pan_iovec_p ma_write_ret(pan_iovec_p p, void **argv);
static void um_write_ret(void *p, void **argv);
#else
static char *ma_write_ret(char *p, void **argv);
static char *um_write_ret(char *p, void **argv);
#endif
static void fr_write_ret(void **argv);

static int handle_operation(t_object *obj, void **argv);

extern tp_dscr read_desc;
extern tp_dscr write_desc;
extern tp_dscr td_handle_operation;

static op_dscr operation_desc[] = {
    {
        0, 0, 
        &read_desc,
        0, 0,
        "copy object (read)",
        sz_read_call, ma_read_call, um_read_call,
        sz_read_ret, ma_read_ret, um_read_ret,
        fr_read_ret
    },
    {
        0, 0,
        &write_desc,
        1, OP_PURE_WRITE,
        "copy object (write)",
        sz_write_call, ma_write_call, um_write_call,
        sz_write_ret, ma_write_ret, um_write_ret,
        fr_write_ret
    },

    {
        handle_operation, 0,
        &td_handle_operation,
        2, OP_BLOCKING, 
        "handle_operation (read)",
        sz_op_call, ma_op_call, um_op_call, 
        sz_retval, ma_retval, um_retval,
        fr_retval
    },
    { 
        0, handle_operation,
        &td_handle_operation,
        3, OP_PURE_WRITE,
        "handle_operation (pure write)",
        sz_op_call, ma_op_call, um_op_call, 
        sz_retval, ma_retval, um_retval,
        fr_retval
    },
    { 
        0, handle_operation,
        &td_handle_operation,
        4, OP_BLOCKING,
        "handle_operation (read-write)",
        sz_op_call, ma_op_call, um_op_call, 
        sz_retval, ma_retval, um_retval,
        fr_retval
    }
};

static par_dscr read_arg_desc[] = {
    { OUT, &obj_desc}
};

static tp_dscr read_desc = { 
    FUNCTION,
    sizeof(t_integer),
    0,
    0,
    1,
    read_arg_desc
};

static par_dscr write_arg_desc[] = {
    { IN, &obj_desc} 
};

static tp_dscr write_desc = { 
    FUNCTION,
    sizeof(t_integer),
    0,
    0,
    1,
    write_arg_desc
};

static par_dscr op_args_desc[] = {
    { IN, &td_integer},     /* pointer to user-defined function */
    { IN, &td_integer},     /* size of output buffer */
    { IN, &td_string},      /* input buffer */
    { OUT, &td_string}      /* output buffer */
};

static tp_dscr td_handle_operation = {
    FUNCTION,               /* this is a function descriptor */
    sizeof(t_integer),      /* size of registration id */
    0,                      /* no flags */
    0,                      /* no return type */
    4,                      /* 4 parameters */
    op_args_desc            /* parameter descriptors */
};

static void call_operation(so_t *obj, so_operation_t op,
                           void *inbuf, int insize,
                           void *outbuf, int outsize, int op_index);

void
so_read_only(so_t *obj, so_operation_t rd_op,
            void *inbuf, int insize, void *outbuf, int outsize)
{
    call_operation(obj, rd_op, inbuf, insize, outbuf, outsize, 2);
}

void
so_pure_write(so_t *obj, so_operation_t wr_op, void *inbuf, int insize)
{
    call_operation(obj, wr_op, inbuf, insize, 0, 0, 3);
}

void
so_read_write(so_t *obj, so_operation_t rw_op,
              void *inbuf, int insize, void *outbuf, int outsize)
{
    call_operation(obj, rw_op, inbuf, insize, outbuf, outsize, 4);
}


static void
call_operation(so_t *obj, so_operation_t op,
             void *inbuf, int insize, void *outbuf, int outsize, int op_index)
{
    void *argv[4];
    t_array tmp_in, tmp_out;
    int op_flags = 0;

    tmp_in.a_data = inbuf;
    tmp_in.a_offset = 0;
    tmp_in.a_sz = insize;
    tmp_in.a_dims[0].a_lwb = 0;
    tmp_in.a_dims[0].a_nel = insize;

    tmp_out.a_data = outbuf;
    tmp_out.a_offset = 0;
    tmp_out.a_sz = outsize;
    tmp_out.a_dims[0].a_lwb = 0;
    tmp_out.a_dims[0].a_nel = outsize;

    argv[0] = (void *)(&op);
    argv[1] = (void *)(&outsize);
    argv[2] = &tmp_in;
    argv[3] = &tmp_out;

    DoOperation((t_object *)obj, &op_flags, &obj_desc, op_index, 0, argv);
}

static int
handle_operation(t_object *obj, void **argv)
{
    void *state, *inbuf, *outbuf;
    so_operation_t user_operation;
    int state_size, insize, outsize;
    so_status_t ret;

    state      = (void *)((t_array *)obj->o_fields)->a_data;
    state_size = (int)((t_array *)obj->o_fields)->a_sz;

    user_operation = *(so_operation_t *)argv[0];

    inbuf   = (void *)((t_array *)argv[2])->a_data;
    insize  = (int)((t_array *)argv[2])->a_sz;

    outsize = *((int *)argv[1]);
    outbuf  = ((t_array *)(argv[3]))->a_data;

    ret = (*user_operation)(state, state_size, inbuf, insize, outbuf, outsize);

    if (ret == SO_BLOCKED) {
        return BLOCKING;
    }
    return 0;
}

#ifdef PANDA4
static int
sz_op_call(void **argv)
{
    return 2 + sz_string(argv[2]);
}

static pan_iovec_p
ma_op_call(pan_iovec_p p, void **argv)
{
    p->data = argv[0];
    p->len = sizeof(int);
    p++;
    p->data = argv[1];
    p->len = sizeof(int);
    p++;
    return ma_string(p, argv[2]);
}

static void
um_op_call(void *p, void ***ap)
{
    void **argv = m_malloc(4 * sizeof(void *));
    int outsize;

    *ap = argv;

    argv[0] = m_malloc(sizeof(t_integer));
    pan_msg_consume(p, argv[0], sizeof(t_integer));

    argv[1] = m_malloc(sizeof(t_integer));
    pan_msg_consume(p, argv[1], sizeof(t_integer));
    outsize = *(int *)(argv[1]);

    argv[2] = m_malloc(sizeof(t_array));
    um_string(p, argv[2]);

    argv[3] = m_malloc(sizeof(t_array));
    a_allocate(argv[3], 1, 1, 0, outsize - 1);
}
#else
static int
sz_op_call(void **argv)
{
    return 2 * sizeof(t_integer) + sz_string(argv[2]);
}

static char *
ma_op_call(char *p, void **argv)
{
    memcpy(p, argv[0], sizeof(t_integer));
    p += sizeof(t_integer);
    memcpy(p, argv[1], sizeof(t_integer));
    p += sizeof(t_integer);
    return ma_string(p, argv[2]);
}

static char *
um_op_call(char *p, void ***ap)
{
    void **argv = m_malloc(4 * sizeof(void *));
    int outsize;

    *ap = argv;

    argv[0] = m_malloc(sizeof(t_integer));
    memcpy(argv[0], p, sizeof(t_integer));
    p += sizeof(t_integer);

    argv[1] = m_malloc(sizeof(t_integer));
    memcpy(argv[1], p, sizeof(t_integer));
    p += sizeof(t_integer);
    outsize = *(int *)(argv[1]);

    argv[2] = m_malloc(sizeof(t_array));
    p = um_string(p, argv[2]);

    argv[3] = m_malloc(sizeof(t_array));
    a_allocate(argv[3], 1, 1, 0, outsize - 1);

    return p;
}
#endif

#ifdef PANDA4
static int
sz_retval(void **argv)
{
    return 1;
}

static pan_iovec_p
ma_retval(pan_iovec_p p, void **argv)
{
    int outsize = *(int *)argv[1];

    p->len = outsize;
    p->data = ((t_array *)argv[3])->a_data;
    return p + 1;
}

static void
um_retval(void *p, void **argv)
{
    int outsize = *(int *)argv[1];

    pan_msg_consume(p, ((t_array *)argv[3])->a_data, outsize);
}
#else
static int
sz_retval(void **argv)
{
    return *(int *)argv[1];
}

static char *
ma_retval(char *p, void **argv)
{
    int outsize = *(int *)argv[1];

    m_free(argv[0]);
    m_free(argv[1]);
    m_free(argv[2]);
    (void)memcpy(p, ((t_array *)argv[3])->a_data, outsize);
    free_string(argv[3]);
    m_free(argv[3]);
    m_free(argv);

    return p + outsize;
}

static char *
um_retval(char *p, void **argv)
{
    int outsize = *(int *)argv[1];

    (void)memcpy(((t_array *)argv[3])->a_data, p, outsize);
    return p + outsize;
}
#endif

static void
fr_retval(void **argv)
{
    m_free(argv[0]);
    m_free(argv[1]);
    m_free(argv[2]);
    free_string(argv[3]);
    m_free(argv[3]);
    m_free(argv);
}

static int
sz_read_call(void **argv) 
{
    return 0;
}

#ifdef PANDA4
static pan_iovec_p
ma_read_call(pan_iovec_p p, void **argv) 
{
    return p;
}

static void
um_read_call(void *p, void ***ap)
{
    void **argv = m_malloc(1*sizeof(void *));
    t_object *o;
    *ap = argv;
    argv[0] = m_malloc(sizeof(t_object));
    o = argv[0];
    o->o_fields = m_malloc(sizeof(t_string));
    memset(o->o_fields, 0, sizeof(t_string));
    o_init_rtsdep(o, &obj_desc, (char *) 0);
}
#else
static char *
ma_read_call(char *p, void **argv) 
{
    return p;
}

static char *
um_read_call(char *p, void ***ap)
{
    void **argv = m_malloc(1*sizeof(void *));
    t_object *o;
    *ap = argv;
    argv[0] = m_malloc(sizeof(t_object));
    o = argv[0];
    o->o_fields = m_malloc(sizeof(t_string));
    memset(o->o_fields, 0, sizeof(t_string));
    o_init_rtsdep(o, &obj_desc, (char *) 0);
    return p;
}
#endif

static int
sz_read_ret(void **argv)
{
    return sz_object(argv[0]);
}

#ifdef PANDA4
static pan_iovec_p
ma_read_ret(pan_iovec_p p, void **argv)
{
    return ma_object(p, argv[0]);
}

static void
um_read_ret(void *p, void **argv)
{
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    um_object(p, argv[0]);
}
#else
static char *
ma_read_ret(char *p, void **argv)
{
    p = ma_object(p, argv[0]);
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    m_free(argv[0]);
    m_free((void *) argv);
    return p;
}

static char *
um_read_ret(char *p, void **argv)
{
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    return um_object(p, argv[0]);
}
#endif

static void
fr_read_ret(void **argv)
{
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    m_free(argv[0]);
    m_free((void *) argv);
}

static int
sz_write_call(void **argv)
{
    return sz_object(argv[0]);
}

#ifdef PANDA4
static pan_iovec_p
ma_write_call(pan_iovec_p p, void **argv)
{
    return ma_object(p, argv[0]);
}

static void
um_write_call(void *p, void ***ap)
{
    void **argv = m_malloc(1*sizeof(void *));
    *ap = argv;
    argv[0] = m_malloc(sizeof(t_object));
    um_object(p, argv[0]);
}
#else
static char *
ma_write_call(char *p, void **argv)
{
    return ma_object(p, argv[0]);
}

static char *
um_write_call(char *p, void ***ap)
{
    void **argv = m_malloc(1*sizeof(void *));
    *ap = argv;
    argv[0] = m_malloc(sizeof(t_object));
    return um_object(p, argv[0]);
}
#endif

static int
sz_write_ret(void **argv)
{
    return 0;
}

#ifdef PANDA4
static pan_iovec_p
ma_write_ret(pan_iovec_p p, void **argv)
{
    return p;
}

static void
um_write_ret(void *p, void **argv)
{
}
#else
static char *
ma_write_ret(char *p, void **argv)
{
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    m_free(argv[0]);
    m_free((void *) argv);
    return p;
}

static char *
um_write_ret(char *p, void **argv)
{
    return p;
}
#endif

static void
fr_write_ret(void **argv)
{
    if (o_free(argv[0])) {
	free_string((t_string *) ((t_object *)(argv[0]))->o_fields);
    }
    m_free(argv[0]);
    m_free((void *) argv);
}
