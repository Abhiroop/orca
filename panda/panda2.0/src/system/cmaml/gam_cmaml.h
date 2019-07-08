#ifndef __gam_cmaml_h__
#define __gam_cmaml_h__

#ifdef LAM

/*
 * This is the part ofthe CMAML interface that is used by
 * our Panda port to CMAML. All functions that are not
 * called by the Panda code have been left out.
 */

extern void gam_cmaml_init(int part_size, int me);
extern void gam_cmaml_end(void);

extern int alloc_port(void *buf, int size,
		      void (*handler)(int rport, void *arg), void *arg2);

extern int CMMD_partition_size(void);
extern int CMMD_self_address(void);
extern void CMMD_error(const char *fmt, ...);
extern void CMMD_sync_with_nodes(void);

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

extern void CMAML_poll_while_lt(volatile int *flag, int value);
extern void CMAML_poll(void);

extern int CMAML_interrupt_status(void);
extern int CMAML_disable_interrupts(void);

#define CMAML_rpc CMAML_reply

extern void CMAML_request(int dest, 
                          void (*handler)(int, int, int, int),
                          int d1, int d2, int d3, int d4);
extern void CMAML_reply(int dest, 
                        void (*handler)(int, int, int, int),
                        int d1, int d2, int d3, int d4);

extern int CMAML_scopy(int dest, int rport, int align, int offset,
                       int addr, int nbytes,
                       void (*sent_handler)(void *arg),
                       void *arg);

extern int CMAML_free_rport(int rport);

#endif /* LAM */

#endif /* __gam_cmaml_h__ */
