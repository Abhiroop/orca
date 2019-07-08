#include "pan_sys.h"
#include "pan_mp.h"
#include "pan_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int msg_size = 20000;
static int rec_map;
static pan_time_p sleep_time;


static void
sender(void)
{
    pan_time_p start, stop;
    pan_msg_p message;
    char *p;
    int i;

    message = pan_msg_create();
    p = (char *)pan_msg_push(message, msg_size, 1);
    for(i = 0; i < msg_size; i++){
	p[i] = (char)(i & 256);
    }

    start = pan_time_create();
    stop  = pan_time_create();

    pan_time_get(start);
    (void)pan_mp_send_message(1, rec_map, message, MODE_SYNC);
    pan_time_get(stop);
    pan_time_sub(stop, start);

    printf("Message sent: %g\n", pan_time_t2d(stop));
}


static void
receiver(void)
{
    pan_msg_p message;
    char *p;
    int i, errn;

    message = pan_msg_create();

    pan_mp_receive_message(rec_map, message, MODE_SYNC);
    
    printf("Message received\n");

    errn = 0;
    p = (char *)pan_msg_pop(message, msg_size, 1);
    for(i = 0; i < msg_size; i++){
	if (p[i] != (char)(i & 256) && errn++ < 30){
	    printf("error: %d %d\n", i, p[i]);
	}
    }
}


int
main(int argc, char *argv[])
{
    pan_init(&argc, argv);

    if (argc > 2){
	fprintf(stderr, "Usage: %s [msg_size]\n", argv[0]);
	exit(1);
    }
    if (argc == 2) msg_size = atoi(argv[1]);

    pan_mp_init();
    pan_util_init();

    rec_map = pan_mp_register_map();

    sleep_time = pan_time_create();
    pan_time_d2t(sleep_time, 5.0);
 
    pan_start();
    
    if (pan_my_pid() == 0){
	sender();
    }else{
	receiver();
    }

    pan_sleep(sleep_time);

    pan_util_end();
    pan_mp_end();

    pan_time_clear(sleep_time);

    pan_end();

    return 0;
}

