#ifndef _SYS_T800_ERROR_
#define _SYS_T800_ERROR_


void pan_sys_error_start(void);

void pan_sys_error_end(void);

void pan_sys_fwrite(char *s, int len);

void pan_sys_printf(const char *fmt, ...);

void pan_sys_flush(void);


#ifdef DEBUG

#define Debug(x) x

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif

#define Debugn(n, x) if (n <= DEBUG_LEVEL) { x; }

#else

#define Debug(x)

#endif

#endif
