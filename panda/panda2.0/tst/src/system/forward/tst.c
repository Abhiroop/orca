#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pan_sys.h"
#include "forward.h"

static char *progname;
static int   nr;
static int   size;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s <nr messages> <message size>\n", progname);
    exit(1);
}
    

static void
sender(void)
{
    pan_msg_p msg;
    char *data;
    int i;

    msg  = pan_msg_init();
    data = (char *)pan_msg_push(msg, size, 1);

    for(i = 0; i < nr; i++){
	pan_saw_send(msg);
    }
}

int
main(int argc, char *argv[])
{
    /* Initialize all modules */
    pan_init(&argc, argv);
    pan_saw_start();
    pan_start();

    progname = argv[0];

    if (argc != 3){
	usage();
    }

    nr   = atoi(argv[1]);
    size = atoi(argv[2]);

    if (pan_my_pid() == 0){
	sender();
    }else{
	sleep(20);
    }

    /* End all modules */
    pan_saw_end();
    pan_end();

    exit(0);
}
