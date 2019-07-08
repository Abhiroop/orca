#include <stdarg.h>

#include "pan_sys_msg.h"
#include "pan_global.h"
#include "pan_error.h"
#include "pan_threads.h"
#include "pan_system.h"

static int dump_core = 0;

void
pan_panic(const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    fprintf(stderr, "thread: %lx@%d \tPanic: ", (long)pan_thread_self(),
    	    pan_my_pid());
    (void)vsprintf(buf, fmt, args); 
    perror(buf);
    va_end(args);
    if (dump_core){
	abort();
    }else{
	exit(-1);
    }
}

void 
pan_sys_dump_core(void)
{
    dump_core = 1 - dump_core;

    /* printf("Panic core dump mode turned %s.\n", dump_core ? "on" : "off");
     */
}
