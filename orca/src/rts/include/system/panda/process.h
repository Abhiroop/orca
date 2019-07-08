#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "orca_types.h"

#define p_get_descr(p)                 ( (p)->pdscr     )
 
#define p_set_blocking(p)              (p)->blocking = 1

#define p_get_blocking(p)              ( (p)->blocking  )

#define p_doing_operation(p)           ( (p)->depth > 0 )

#define p_create_process(descr, argv)  (void)p_init((thread_t)0, descr, argv)

#define p_clear(p)


extern void p_start(void);

extern void p_end(void);

extern process_p p_init(thread_t tid, struct proc_descr *orca_descr,
			void **argv);

extern process_p p_self(void);

#define p_push_object(self) \
  do { \
      (self)->blocking = 0; \
      (self)->depth++; \
  } while (0)

extern int p_pop_object(process_p self);


#endif
