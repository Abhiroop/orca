#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "pan_sys.h"

#define PANDA_MSG_LIMIT LONG_MAX


int
main(int argc, char *argv[])
{
    int         max_msg_size;
    int         nsamples;
    int         step_size;
    int         i;
    int         j;
    pan_msg_p   msg;
    void       *p;
    pan_time_p  test_start = pan_time_create();
    pan_time_p  test_stop = pan_time_create();

    if (argc < 4) {
	fprintf(stderr, "Usage: %s <nsamples> <max_msg_size> <step_size>\n",
		argv[0]);
	exit(1);
    }

    pan_init(&argc, argv);

    if (argc != 4) {
	fprintf(stderr, "Usage: %s <nsamples> <max_msg_size> <step_size>\n",
		argv[0]);
	exit(1);
    }

    nsamples = atoi(argv[1]);
    max_msg_size = atoi(argv[2]);
    step_size = atoi(argv[3]);

    if (max_msg_size > PANDA_MSG_LIMIT) {
	fprintf(stderr, "specify size less than panda msg limit: %ld\n",
		PANDA_MSG_LIMIT);
	exit(1);
    }

    for (i = 0; i <= max_msg_size; i += step_size) {
	pan_time_get(test_start);
	for (j = 0; j < nsamples; j++) {
	    msg = pan_msg_create();
	    p = pan_msg_push(msg, i, 1);
	    pan_msg_clear(msg);
	}
	pan_time_get(test_stop);
	pan_time_sub(test_stop, test_start);
	printf("%d samples building msg of size %d =  %f sec\n",
	       nsamples, i, pan_time_t2d(test_stop));

	pan_time_get(test_start);
	for (j = 0; j < nsamples; j++) {
	    msg = pan_msg_create();
	    p = pan_msg_push(msg, i, 1);
	    memset(p, 'a', i);
	    pan_msg_clear(msg);
	}
	pan_time_get(test_stop);
	pan_time_sub(test_stop, test_start);
	printf("%d samples building + filling msg of size %d =  %f sec\n",
	       nsamples, i, pan_time_t2d(test_stop));
    }

    pan_time_clear(test_start);
    pan_time_clear(test_stop);

    pan_end();

    return 0;
}
