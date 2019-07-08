#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pan_sys.h"
#include "pan_sys_amoeba_wrapper.h"
#include "blist.h"

typedef struct node {
        char        *name;
        int          gid;
        struct node *next;
} node_t, *node_p;

struct b_list_t {
        node_p  tail;
};


void b_create(b_list_p *b_list)
{
    *b_list = (b_list_p) sys_malloc((size_t) sizeof(b_list_t));
    (*b_list)->tail = NULL;
}


void b_destroy(b_list_p b_list)
{
    node_p p, q;

    if (b_list->tail != NULL) {
        p = b_list->tail->next;
        while (p != b_list->tail) {
             q = p;
             p = p->next;
             free(q->name);
             free(q);
        }
        free(p->name);
        free(p);
    }
    free(b_list);
}


static int b_empty(b_list_p b_list)
{
    return (b_list->tail == NULL ? 1 : 0);
}


int b_find(b_list_p b_list, char *name, int *gid)
{
    node_p p;

    if (b_empty(b_list)) {
        return 0;
    }

    p = b_list->tail;
    do {
        if (strcmp(p->name, name) == 0) {
            *gid = p->gid;
            return 1;
        }
        p = p->next;
    }
    while (p != b_list->tail);

    return 0;
}


void b_del(b_list_p b_list, int gid)
{
    node_p p,q;

    if (b_empty(b_list)) return;

    p = b_list->tail;
    while ((p->next->gid != gid) && (p->next != b_list->tail)) {
            p = p->next;
    }

    if (p->next->gid == gid) {
        q = p->next;
        p->next = q->next;
        if (q == b_list->tail)
            b_list->tail = (q == p) ? NULL: p;
        free(q->name);
        free(q);
    }
    else {
        sys_panic("b_del %d", gid);
    }
}


void b_enter(b_list_p b_list, char *name, int gid)
{
    node_p p;

    p = (node_p) sys_malloc((size_t) sizeof(node_t));
    p->name = (char *) sys_malloc((size_t) strlen(name) + 1);
    (void) strcpy(p->name, name);
    p->gid = gid;

#ifdef VERBOSE
    Printf("entered %s %d\n", p->name, p->gid);
#endif
    if (b_list->tail == NULL) {
        p->next = p;
    }
    else {
        p->next = b_list->tail->next;
        b_list->tail->next = p;
    }
    b_list->tail = p;
}
