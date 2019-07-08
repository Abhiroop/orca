#ifndef _SYS_CMAML_MALLOC_
#define _SYS_CMAML_MALLOC_

void pan_malloc_start(int size_wanted);
void pan_malloc_stats(void);
char *pan_sbrk(int incr);

#endif /* _SYS_CMAML_MALLOC_ */
