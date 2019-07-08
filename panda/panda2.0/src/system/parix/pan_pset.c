#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_pset.h"
#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include <assert.h>
#include <string.h>
#include <limits.h>

#define UN_BIT (sizeof(unsigned) * CHAR_BIT)

static int masksize;

#define SETBIT(mask, x)    (mask[x / UN_BIT] |=  (1 << (x % UN_BIT)))
#define CLEARBIT(mask, x)  (mask[x / UN_BIT] &= ~(1 << (x % UN_BIT)))
#define TESTBIT(mask, x)   (mask[x / UN_BIT] &   (1 << (x % UN_BIT)))

void
pan_sys_pset_start(void)
{
    masksize = pan_sys_nr / UN_BIT + 1; /* 1 for boundary */
}


void
pan_sys_pset_end(void)
{
}


pan_pset_p
pan_pset_create(void)
{
    pan_pset_p pset;

    pset = (pan_pset_p)pan_malloc(sizeof(struct pan_pset));

    pset->mask = (unsigned *)pan_malloc(masksize * sizeof(unsigned));
    pan_pset_empty(pset);

    return pset;
}


void
pan_pset_clear(pan_pset_p pset)
{
    pan_free(pset->mask);
    pan_free(pset);
}


int
pan_pset_isempty(pan_pset_p pset)
{
    int i;

    for (i = 0; i < masksize; i++) {
	if (pset->mask[i]) {
	    return 0;
	}
    }
    return 1;
}


void
pan_pset_add(pan_pset_p pset, int pid)
{
    SETBIT(pset->mask, pid);
}


void pan_pset_del(pan_pset_p pset, int pid)
{
    CLEARBIT(pset->mask, pid);
}


void
pan_pset_fill(pan_pset_p pset)
{
    int i;

    for(i = 0; i < masksize; i++){
	pset->mask[i] = UINT_MAX;
    }
}


int
pan_pset_find(pan_pset_p pset, int pid_offset)
{
    int i;

    for (i = pid_offset; i < pan_sys_nr; i++) {
	if (TESTBIT(pset->mask, i)) return i;
    }

    return -1;
}


void
pan_pset_empty(pan_pset_p pset)
{
    int i;

    for(i = 0; i < masksize; i++){
	pset->mask[i] = 0;
    }
}


void
pan_pset_copy(pan_pset_p pset, pan_pset_p copy)
{
    int i;

    for(i = 0; i < masksize; i++){
	copy->mask[i] = pset->mask[i];
    }
}


int
pan_pset_ismember(pan_pset_p pset, int pid)
{
    return TESTBIT(pset->mask, pid) ? 1 : 0;
}


int
pan_pset_size(void)
{
    return masksize * sizeof(unsigned);
}


void
pan_pset_marshall(pan_pset_p pset, void *buffer)
{
    int i;
    unsigned *b = buffer;

    for(i = 0; i < masksize; i++){
	b[i] = pset->mask[i];
    }
}


void
pan_pset_unmarshall(pan_pset_p pset, void *buffer)
{
    int i;
    unsigned *b = buffer;

    for(i = 0; i < masksize; i++){
	pset->mask[i] = b[i];
    }
}
    

int
pan_pset_nr_members(pan_pset_p pset)
{
    int i;
    int b;
    int n = 0;
    unsigned int set_word;

    for (i = 0; i < masksize; i++) {
	set_word = pset->mask[i];
	for (b = 0; b < UN_BIT; b++) {
	    if (set_word & 0x1) {
		++n;
	    }
	    set_word >>= 1;
	}
    }
    return n;
}


/*$char *
pan_print_pset(pan_pset_p pset)
{
    int i;
    int b;
    int n;
    char byte;
    static char *str_buf = NULL;
    char *buf_ptr;

    if (str_buf == NULL) {
	i = 1;$*/		/* #decimals */
	/*$n = 10;$*/		/* 10 ^ i */
	/*$while (n < masksize * UN_BIT) {
	    i++;
	    n *= 10;
	}
	str_buf = pan_malloc((i + 1) * UN_BIT * masksize + 2);
    }
    buf_ptr = str_buf;
    sprintf(buf_ptr, "[");
    buf_ptr = strchr(buf_ptr, '\0');
    n = 0;
    for (i = 0; i < masksize; i++) {
	byte = pset->mask[i];
	for (b = 0; b < UN_BIT; b++) {
	    if (byte & 0x1) {
		sprintf(buf_ptr, "%d,", n);
		buf_ptr = strchr(buf_ptr, '\0');
	    }
	    byte >>= 1;
	    ++n;
	}
    }
    sprintf(buf_ptr, "]");
    return str_buf;
}$*/
