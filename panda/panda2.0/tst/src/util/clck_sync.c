/* Test program for the clock sync protocol.
 * Parameters:
 * test_1 my_platform n_platforms n_syncs
 * watch out: 1 <= my_platform <= n_platforms
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pan_sys.h"

#include "pan_util.h"



int          main(int argc, char** argv)
{
    int          n_syncs;
    pan_time_p   clock_shift = pan_time_create();
    pan_time_p   stddev = pan_time_create();

    if (argc < 2) {
	fprintf(stderr, "usage %s n_syncs\n", argv[0]);
	exit(1);
    }

    printf("%2d: before pan_init...\n", pan_my_pid());
    pan_init(&argc, argv);
    printf("%2d: past pan_init...\n", pan_my_pid());

    if (argc < 2) {
	fprintf(stderr, "usage %s n_syncs\n", argv[0]);
	exit(1);
    }

    n_syncs = atoi(argv[1]);

    pan_util_init();

    pan_start();

    n_syncs = pan_clock_sync(n_syncs, clock_shift, stddev);
    printf("%2d: clock shift = %f +- %f from %d usable exchanges\n",
	   pan_my_pid(), pan_time_t2d(clock_shift), pan_time_t2d(stddev),
	   n_syncs);

    pan_end();

    pan_time_clear(clock_shift);
    pan_time_clear(stddev);

    return(0);
}
