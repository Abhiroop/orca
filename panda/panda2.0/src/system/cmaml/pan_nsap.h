#ifndef _SYS_CMAML_NSAP_
#define _SYS_CMAML_NSAP_

#include "pan_fragment.h"

typedef enum{
    PAN_NSAP_FRAGMENT,
    PAN_NSAP_SMALL
}pan_nsap_addr_t;


struct pan_nsap{
    pan_nsap_addr_t  type;	/* type of nsap */
    int              nsap_id;	/* nsap id used to identify handler */
    int              flags;	/* usage flags (unicast, multicast) */

    /* For type == PAN_NSAP_FRAGMENT */
    void           (*rec_frag)(pan_fragment_p frag);
				/* receive handler */
    int              hdr_size;	/* header size  */

    /* For type == PAN_NSAP_SMALL */
    int              mapid;
    void           (*rec_small)(void *data);
				/* receive handler */
    int              data_len;	/* data size */
};

extern pan_nsap_p pan_nsap_map[];
extern int pan_small_nr;

extern void       pan_sys_nsap_start(void);
extern void       pan_sys_nsap_end(void);
extern pan_nsap_p pan_sys_nsap_id(int id);

#define pan_sys_nsap_index(nsap)  ((nsap)->nsap_id)

#endif /* _SYS_CMAML_NSAP_ */
