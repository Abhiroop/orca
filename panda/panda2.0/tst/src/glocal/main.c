#include <stdio.h>
#include "panda/panda.h"

static void f(void)
{
    int *ip = (int *)sys_glocal_data();

    (void)Printf("my glocal data: %d\n", *ip);
}

static void *thread(void *arg)
{
    int *ip = (int *)sys_glocal_malloc(sizeof(int));

    *ip = (int)arg;
    f();

    sys_glocal_free();

    sys_thread_exit((void *)0);
}

void main(int argc, char **argv)
{
    thread_t t1, t2;
    
    pan_init(1, 1);

    sys_thread_create(&t1, thread, (void *)1, 0, 0);
    sys_thread_create(&t2, thread, (void *)2, 0, 0);
    sys_thread_join(&t1);
    sys_thread_join(&t2);

    pan_end();
}

