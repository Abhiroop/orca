#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/sem.h>
#include <sys/logerror.h>
#include <sys/sys_rpc.h>

#include "pan_sys.h"

#include "pan_system.h"
#include "pan_threads.h"
#include "pan_error.h"


static int       core_on_panic = 0;
static pan_key_p printf_key;

typedef struct PRINTF_BUF {
    int    buf_size;
    char  *buf;
    char  *current;
} print_buf_t, *print_buf_p;


void
pan_sys_dump_core(void)
{
    core_on_panic = 1;
}

void
pan_SimplePanic(char *event, char *file, int line)
{
    printe("CPU: %d, Node: %d, Thread: 0x%x, File: %s, Line: %d, Panic: %s\n",
	 pan_my_pid(), pan_my_pid(), pan_thread_self(), file, line, event);

    if (core_on_panic) {
	assert(0);
    }
#ifdef NDEBUG
    /* pan_thread_exit( NULL );     which 'exit' use else? */
    AbortServer(1);
#else
    abort();
#endif
}

void
pan_panic(const char *fmt,...)
{
    va_list     args;
    char        buf[256];

    va_start(args, fmt);
    sprintf(buf, "%2d/0x%x Panic:\t", pan_my_pid(), pan_thread_self());
    vsprintf(strchr(buf, '\0'), fmt, args);
    /* printe("CPU: %d, Node: %d, Thread: 0x%x, Panic: %s",
	   pan_my_pid(), pan_my_pid(), pan_thread_self(), buf); */
    printe("%s", buf);
    va_end(args);

    if (core_on_panic) {
	assert(0);
    }

#ifdef NDEBUG
    /* pan_thread_exit( NULL );     which 'exit' use else? */
    AbortServer(1);
#else
    abort();			/* does this halt in debugger? */
#endif
}


void
pan_sys_fwrite(char *s, int len)
{
    print_buf_p pbuf;

    pbuf = pan_key_getspecific(printf_key);
    if (pbuf == NULL) {
	pbuf = pan_malloc(sizeof(print_buf_t));
	pbuf->buf_size = 256;
	pbuf->buf = pan_malloc(pbuf->buf_size);
	pbuf->current = pbuf->buf;
	pan_key_setspecific(printf_key, pbuf);
    }
    if (pbuf->buf_size - (pbuf->current - pbuf->buf) < len + 1) {
	while (pbuf->buf_size - (pbuf->current - pbuf->buf) < len + 1) {
	    pbuf->buf_size += 256;
	}
	pbuf->buf = pan_realloc(pbuf->buf, pbuf->buf_size);
    }

    if (pbuf->current == pbuf->buf) {
	sprintf(pbuf->current, "%2d/%2d/0x%x\t",
		pan_my_pid(), pan_sys_Parix_id, pan_thread_self());
	pbuf->current = strchr(pbuf->current, '\0');
    }
    memcpy(pbuf->current, s, len);
    pbuf->current += len;
    *pbuf->current = '\0';

    if (strchr(pbuf->buf, '\n') != NULL) {
	printe(pbuf->buf);
	pbuf->current = pbuf->buf;
    }
}


void
pan_sys_printf(const char *fmt,...)
{
    va_list     args;
    print_buf_p pbuf;

    pbuf = pan_key_getspecific(printf_key);
    if (pbuf == NULL) {
	pbuf = pan_malloc(sizeof(print_buf_t));
	pbuf->buf_size = 256;
	pbuf->buf = pan_malloc(pbuf->buf_size);
	pbuf->current = pbuf->buf;
	pan_key_setspecific(printf_key, pbuf);
    }

    if (pbuf->current == pbuf->buf) {
	sprintf(pbuf->current, "%2d/%2d/0x%x\t",
		pan_my_pid(), pan_sys_Parix_id, pan_thread_self());
	pbuf->current = strchr(pbuf->current, '\0');
    }

    va_start(args, fmt);
    vsprintf(pbuf->current, fmt, args);
    pbuf->current = strchr(pbuf->current, '\0');
    /* printe("CPU: %d, Node: %d, Thread: 0x%x, Printf: %s",
	   pan_my_pid(), pan_my_pid(), pan_thread_self(), buf); */
    va_end(args);

    if (strchr(pbuf->buf, '\n') != NULL) {
	printe(pbuf->buf);
	pbuf->current = pbuf->buf;
    }
}


void
pan_sys_flush(void)
{
    print_buf_p pbuf;

    pbuf = pan_key_getspecific(printf_key);
    if (pbuf != NULL && pbuf->current != pbuf->buf) {
	printe(pbuf->buf);
	pbuf->current = pbuf->buf;
    }
}


void
pan_sys_error_start(void)
{
    printf_key = pan_key_create();
}


void
pan_sys_error_end(void)
{
    pan_key_clear(printf_key);
}


#ifndef ASSERT_STOP

#ifdef PARIX_T800

void
_assert(const char *expr, const char *file, unsigned int line)
{
    pan_sys_printf("%s:%d\tassertion fails: %s\n", file, line, expr);
}

#else

int
__assert_fail(const char *expr, const char *file, unsigned int line)
{
    char *p;

    pan_sys_printf("%s:%d\tassertion fails: %s\n", file, line, expr);
#ifdef USE_DEBUG_STOP
    DebugStop();
#else
    p = NULL;
    if (*p) {
	return -99;
    }
#endif
    return -1;
}



#endif


#endif
