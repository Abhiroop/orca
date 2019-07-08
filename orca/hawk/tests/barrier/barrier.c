#include <stdlib.h>

#include "synchronization.h"
#include "communication.h"
#include "collection.h"
#include "util.h"

#define NODUMP

static int me;
static int group_size;
static int proc_debug;
  
static int iter = 100;
static po_timer_p timer;

int 
main(int argc, char *argv[])
{
    channel_p ch;
    set_p group;
    int i;

    printf("init rts\n");
    init_rts(&argc, argv);
    get_group_configuration(&me, &group_size, &proc_debug);

    if (argc == 2) iter = atoi(argv[1]);

    group = new_set(group_size);
    full_set(group);

    ch = new_channel(barrier, binomial_tree, 0, NULL, group);

    timer = new_po_timer("barrier");

    if (me == 0) printf("Performing %d barriers\n", iter);


    printf("Starting\n");
    /* Wait until all machines are initialized */
    barrier(ch);
    wait_on_channel(ch);

    start_po_timer(timer);
    for(i = 0; i < iter; i++) {
	barrier(ch);
	wait_on_channel(ch);
#ifdef DUMP
	printf("%d: after iteration %d\n", me, i);
#endif
    }
    end_po_timer(timer);

    if (me == 0) printf("%d barriers on %d machines took %f seconds\n",
			iter, group_size, read_last(timer, PO_SEC));

    sleep(2);
    free_channel(ch);
    free_po_timer(timer);

    free_set(group);

    printf("Before finish rts\n");
    finish_rts();
    printf("After finish rts\n");

    return 0;
}
  
