#ifndef __B_LIST__
#define __B_LIST__

#define b_list_t	pan_b_list_t
#define b_list_p	pan_b_list_p

#define b_create	pan_b_create
#define b_find		pan_b_find
#define b_enter		pan_b_enter
#define b_del		pan_b_del
#define b_destroy	pan_b_destroy


typedef struct b_list_t b_list_t, *b_list_p;

extern void b_create(b_list_p *bindings);
extern int  b_find(b_list_p bindings, char *name, int *gid);
extern void b_enter(b_list_p bindings, char *name, int gid);
extern void b_del(b_list_p bindings, int gid);
extern void b_destroy(b_list_p bindings);

#endif

