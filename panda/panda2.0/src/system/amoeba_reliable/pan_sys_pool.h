#ifndef _SYS_GENERIC_POOL_
#define _SYS_GENERIC_POOL_

#include "pan_sys.h"

typedef enum {EXTERNAL_ENTRY = 0, IN_POOL_ENTRY, OUT_POOL_ENTRY} pool_mode_t;

#define POOL_HEAD \
struct pool_entry *pool_next; \
pool_mode_t        pool_mode

typedef struct pool_entry{
    POOL_HEAD;
}pool_entry_t, *pool_entry_p;

typedef enum {POLICY_NORMAL} policy_t;

typedef pool_entry_p (*create_f)(void);
typedef void         (*clear_f)(pool_entry_p entry);

typedef struct pool{
    pool_entry_p head;
    int          nr;
    int          out;
    pan_mutex_p  lock;
    create_f     create;
    clear_f      clear;
    policy_t     policy;

#ifdef STATISTICS
    /* For debugging and statistics */
    char       *name;
    int         nr_created;
    int         nr_cleared;
    int         nr_get;
    int         nr_put;
#endif

}pool_t, *pool_p;


void pan_sys_pool_start(void);
void pan_sys_pool_end(void);

void         pan_sys_pool_init(pool_p pool, policy_t policy, int size, 
			       create_f create, clear_f clear, char *name);
void         pan_sys_pool_clear(pool_p pool);
pool_entry_p pan_sys_pool_get(pool_p pool);
void         pan_sys_pool_put(pool_p pool, pool_entry_p entry);
void         pan_sys_pool_statistics(void);

#define pan_sys_pool_mark(e, mode) ((e)->pool_mode = (mode))

#endif /* _SYS_GENERIC_POOL_ */
