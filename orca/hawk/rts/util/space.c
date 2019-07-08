#include <stdlib.h>
#include "space.h"

#if defined(__bsdi__)

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

static void *space;

struct spacelist {
    void *addr;
    struct spacelist *next;
    size_t sz;
    char isfree;
};

static struct spacelist *spacelist;

static int initialized;
static int use_mmap = 0;
static int nmegs = 256;

#define PAGESZ	4096
#define M	(1024*1024)

int init_space_module(int moi, int gsize, int pdebug, int *argc, char **argv)
{
    int flags = MAP_PRIVATE|MAP_NORESERVE;
    int fd;
    int i;
    int newargc;

    if (initialized) return 0;
    newargc = 1;
    for (i= 1; i < *argc; i++) {
	if (0) {
	} else if (strcmp(argv[i], "-hawk_use_mmap") == 0) {
	    if (moi == 0) printf("Using mmap for partitioned object space\n");
	    use_mmap = 1;
	} else if (strcmp(argv[i], "-hawk_mmap_megs") == 0) {
	    nmegs = atoi(argv[i+1]);
	    i++;
	} else {
	    argv[newargc++] = argv[i];
	}
    }
    *argc = newargc;
    argv[newargc] = 0;
    initialized = 1;

    if (! use_mmap) return 0;

    fd = open("/dev/zero", O_RDWR);

    if (fd == -1) flags |= MAP_ANON;
    else flags |= MAP_FILE;
    space = mmap((caddr_t) 0, nmegs*M, PROT_READ|PROT_WRITE, flags, fd, 0);
    if (space == (caddr_t)-1) {
	fprintf(stderr, "init_space_module: mmap failed\n");
	exit(1);
    }
    if (fd != -1) close(fd);

    spacelist = malloc(sizeof(struct spacelist));
    if (spacelist == 0) {
	fprintf(stderr, "init_space_module: malloc failed\n");
	exit(1);
    }
    spacelist->isfree = 1;
    spacelist->next = 0;
    spacelist->sz = nmegs*M;
    spacelist->addr = space;
    return 0;
}

int finish_space_module(void)
{
    struct spacelist *l = spacelist;

    if (! initialized || ! use_mmap) return 0;

    while (l != 0) {
	l = l->next;
	free(spacelist);
	spacelist = l;
    }
    munmap(space, nmegs*M);
    return 0;
}

void *get_space(size_t size)
{
    struct spacelist *l = spacelist;
    struct spacelist *new;

    if (! use_mmap) return malloc(size);
    size = (size + (PAGESZ-1)) & ~(PAGESZ-1);
    while (l && (! l->isfree || l->sz < size)) l = l->next;
    if (! l) return 0;
    if (l->sz == size) {
	l->isfree = 0;
	return l->addr;
    }
    new = malloc(sizeof(struct spacelist));
    if (! new) return 0;
    new->isfree = 1;
    new->next = l->next;
    new->sz = l->sz - size;
    new->addr = (char *)(l->addr) + size;
    l->isfree = 0;
    l->next = new;
    return l->addr;
}

void free_space(void *p)
{
    struct spacelist *l = spacelist, *l1 = 0;
    
    if (! use_mmap) {
	free(p);
	return;
    }
    while (l != 0 && l->addr != p) {
	l1 = l;
	l = l->next;
    }
    assert(l);
    assert(! l->isfree);
    l->isfree = 1;
    if (l->next && l->next->isfree) {
	struct spacelist *ln = l->next;
	l->sz += ln->sz;
	l->next = ln->next;
	free(ln);
    }
    if (l1 && l1->isfree) {
	l1->sz += l->sz;
	l1->next = l->next;
	free(l);
    }
}
#else
int init_space_module(int moi, int gsize, int pdebug, int *argc, char **argv)
{
    return 0;
}

int finish_space_module(void)
{
    return 0;
}

void *get_space(size_t size)
{
    return malloc(size);
}

void free_space(void *p)
{
    free(p);
}
#endif
