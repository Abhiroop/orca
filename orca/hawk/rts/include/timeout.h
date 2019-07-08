#ifndef __timeout__
#define __timeout__

typedef struct timeout_s timeout_t, *timeout_p;

int init_timeout(int,int,int, int *argc, char **argv);
int finish_timeout(void);
timeout_p new_timeout(int mint, int intt, int maxt);
int free_timeout(timeout_p t);
int do_timeout(timeout_p timeout);
int cancel_timeout(timeout_p timeout);
int start_timeout(timeout_p t);
int reset_timeout(timeout_p t);
int getmaxt(timeout_p t);

#endif
