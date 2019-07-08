#ifndef _SYS_GENERIC_NSAP_
#define _SYS_GENERIC_NSAP_

typedef enum{
    PAN_NSAP_FRAGMENT,
    PAN_NSAP_SMALL
}pan_nsap_addr_t;

typedef int	nsap_t, *nsap_p;

struct pan_nsap{
    pan_nsap_addr_t  type;	/* type of nsap */
    nsap_t           nsap_id;	/* nsap id used to identify handler */
    int              flags;	/* usage flags (unicast, multicast) */

    /* For type == PAN_NSAP_FRAGMENT */
    void           (*rec_frag)(pan_fragment_p);
				/* receive handler */
    int              hdr_size;	/* header size  */

    /* For type == PAN_NSAP_SMALL */
    void           (*rec_small)(void *);
				/* receive handler */
    int              data_len;	/* data size */
    int              comm_hdr_size;	/* size of communication hdr */
};



#define NSAP_HDR_SIZE sizeof(nsap_t)

extern void       pan_sys_nsap_start(void);
extern void       pan_sys_nsap_end(void);
extern pan_nsap_p pan_sys_nsap_id(int id);
extern void       pan_sys_nsap_comm_hdr(pan_nsap_p nsap, int comm_hdr_size);

#define pan_sys_nsap_index(nsap)  ((nsap)->nsap_id)

#endif

