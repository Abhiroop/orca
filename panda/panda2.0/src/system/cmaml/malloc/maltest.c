#include	"param.h"	/* for CHECK */

#define	N		40000	/* maximum number of allocates ever */
#define	LOG_SIZE	10

extern char *sbrk();
extern char *malloc(), *realloc();
extern long atol();
extern free();

#define	Malloc(u)	malloc(u)
#define	Free(u)		free(u)
#define	Realloc(a,s)	realloc(a, s)

struct chunk *getfreechunk(), **getrandchunkhook();
long dev();
int rrand(), logrand();

int n = N;		/* number of allocates now */
char real = 1;		/* set to 0 for timing without mallocking */

void
main(int argc, char **argv) {
	int i, gr = 0, rel = 0;
	char *old_end;
	long r = 0;
	
	while (argc > 1 && argv[1][0] == '-')	{
		switch (argv[1][1])	{
		case 'n':
			n = atoi(&argv[1][2]);
			if (n > N)	{
				printf("maltest: -n%d ", n);
				error("too large", 0);
			}
			break;
		case 'r':
			r = atol(&argv[1][2]);
			break;
		case 't':
			real = 0;		/* just timing the dummy */
			break;
		default:
			error("Call is: mal [-n<number> -r<random> -t]", 0);
		}
		argc--, argv++;
	}
	if (argc != 1)
		error("Call is: mal [-nN -t]", 0);

	
	old_end = sbrk(0);
	if (r)
		srand(r);
	initchunks();
	/*	Simulate a sequence of Grabs slowly changing into a
		sequence of Releases by approximating a circle on a
		square grid.
		Every 10 actions a Change is done, just to liven things
		up a bit.
	*/
	for (i = 0; i < 2*n; i++)	{
		if (i % 10 == 1)	{
			/* so it will happen to the first block already */
			Change();
		}
		if (dev(gr+1, rel) < dev(gr, rel+1))	{
			Grab();
			gr++;
		}
		else	{
			Release();
			rel++;
		}
	}
	printf("n = %d: %ld bytes occupied\n", n, (long)(sbrk(0) - old_end));
#ifdef	CHECK
	maldump(0);
#endif	/* CHECK */
	exit(0);
}

long
dev(int g, int r)	{
	/* Distance of (g, r) from circle around (0, n) with radius n */
	long y = n - r;
	long d2 = g*g + y*y;
	long n2 = n*n;
	
	return d2 > n2 ? d2 - n2 : n2 - d2;
}

/*	Randoms */
int
rrand(void)	{
	/* An improved random */
	return rand() >> 5;
}

int
logrand(int n)	{
	/*	A roughly logarithmically distributed variable
		between 1 and 2**n-1.
	*/
	int normbit = 1 << (rrand() % n);
	return (rrand() & (normbit-1)) + normbit;
}

/*	Chunks */
/*	Each allocated chunk is kept track of in a list of struct chunk's.
*/
struct chunk	{
	struct chunk *next;
	unsigned int size;
	char *addr;
	char mark;
};

#define	N_CHUNKS	(N/2)	/* N * (sqrt(2)-1) is sufficient */
struct chunk chunk[N_CHUNKS];
struct chunk *ffree;	/* start of free list */
struct chunk *focc;	/* start of occupied list */
struct chunk *frels;	/* pointer to chunk after the one just freed */

void
initchunks(void)	{
	/* The free list of chunks is initialised. */
	int i;
	
	focc = frels = 0;
	ffree = &chunk[0];
	for (i = 0; i < N_CHUNKS-1; i++)
		chunk[i].next = &chunk[i+1];
}

struct chunk *
getfreechunk(void)	{
	/* Gets a free chunk, to be occupied immediately */
	struct chunk *new = ffree;
	
	if (!ffree)
		error("out of chunks", 1);
	ffree = new->next;
	/* link into occupied-list */
	new->next = focc;
	focc = new;
	return new;
}

struct chunk **
getrandchunkhook(void)	{
	/*	Returns a hook to the address of a random OCCUPIED chunk.
		A hook is provided rather than a pointer to speed up the
		subsequent freechunk(hook).
	*/
	struct chunk **hook = &frels;
	int i;
	
	/* obtained by following the occupied list for 10 steps */
	if (!*hook)
		hook = &focc;
	if (!*hook)
		error("no chunk found", 1);
	for (i = 0; i < 10; i++)	{
		hook = &(*hook)->next;
		if (!*hook)
			hook = &focc;
	}
	return hook;
}

void
freechunkhook(struct chunk **chp)	{
	/* Free the chunk that hangs on the hook chp */
	struct chunk *ch = *chp;	/* the chunk itself */
	
	*chp = ch->next;		/* remove from chain */
	frels = ch->next;		/* starting point for next release */
	ch->next = ffree;
	ffree = ch;
}

void
printchain(char *msg, struct chunk *p)	{
	/*	Prints the contents of the chunk chain starting at p.
		If all != 0, all info is printed, otherwise entries
		are just listed.
	*/
	int cnt = 1;
	
	printf("%s:", msg);
	printf(all ? "\n": " ");
	for ( ; p; p = p->next)	{
		printf("%2d: %6lo", cnt++, (long)p);
		if (all)
			printf(", size = %u, addr = %ld, mark = %c\n",
					p->size, (long)p->addr, p->mark);
		else	printf("; ");
	}
	if (!all)
		printf("\n");
}

void
Print(char *msg)	{
	/* prints the occupied- and free-lists in readable format. */
	printf("%s:\n", msg);
	printchain("Occ", focc, 1);
	printchain("Free", ffree, 0);
	printf("\n");
}

/*	Allocating */
char dummy[1<<LOG_SIZE];		/* for use if !real */

void
Grab(void)	{
	unsigned int size = logrand(LOG_SIZE);
	char *addr = real ? Malloc(size) : dummy;
	char mark = (rrand() % 95) + ' ';
	struct chunk *fch = getfreechunk();
	int i;
	
	if (!addr)
		error("out of memory", 1);
	for (i = 0; i < size; i++)
		addr[i] = mark;
	fch->size = size;
	fch->addr = addr;
	fch->mark = mark;
}

void
Release(void)	{
	struct chunk **rchp = getrandchunkhook();
	struct chunk *rch = *rchp;		/* the chunk to be released */
	unsigned int size = rch->size;
	char *addr = rch->addr;
	char mark = rch->mark;
	int i;
	
	/* test its contents */
	for (i = 0; i < size; i++)
		if (addr[i] != mark && real)	{
			printf("area was: ");
			for (i = 0; i < size; i++)
				putchar(mark);
			printf("\n");
			printf("area is:  ");
			for (i = 0; i < size; i++)
				putchar(addr[i]);
			printf("\n");
			error("area corrupted", 1);
		}
	if (real)
		Free(addr);
	freechunkhook(rchp);
}

void
Change(void)	{
	struct chunk **rchp = getrandchunkhook();
	struct chunk *rch = *rchp;		/* the chunk to be released */
	unsigned int old_size = rch->size;
	char *addr = rch->addr;
	char mark = rch->mark;
	unsigned int new_size = logrand(LOG_SIZE);
	unsigned int min_size;
	int i;
	
	/* test its contents */
	for (i = 0; i < old_size; i++)
		if (addr[i] != mark && real)	{
			error("old area corrupted", 1);
		}
	
	/* realloc */
	addr = real ? Realloc(addr, new_size) : dummy;
	if (!addr)
		error("out of memory", 1);
	min_size = old_size < new_size ? old_size : new_size;
	
	/* test its contents again */
	for (i = 0; i < min_size; i++)
		if (addr[i] != mark && real)	{
			error("reallocked area corrupted", 1);
		}
	for (i = 0; i < new_size; i++)
		addr[i] = mark;
	rch->size = new_size;
	rch->addr = addr;
}

/*	Error reporting */

void
error(char *s, int ab) {
	printf(ab ? "maltest: %s\n" : "%s\n", s);
	if (ab)
		abort();
	else	exit(1);
}
