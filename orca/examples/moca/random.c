/*** Please don't laugh too loud, while looking at this code... ***/

#include <stdio.h>
#include <stdlib.h>

#define	NR_LINES	(atoi(argl[1]))


int main(argc, argl)
int	argc;
char	**argl;
{
	int	i, j, k;

	printf("          %d\n", NR_LINES * 2);
	for (i = 0; i < NR_LINES; i++) {
	    for (j = 0; j < 3; j++) {
		(rand() % 10) % 3 ? printf("  ") : printf(" -");
		printf("%d.", (int) (drand48() * 10));
		for (k = 0; k < 7; k++)
		    printf("%d", rand() % 10);
		printf("E-%02d", (int) (drand48() * 4 + 9));
	    }
	    printf("\n");
	}
	for (i = 0; i < 9; i++) {
	    printf("           2\n");
	    printf("  4.4141102E-10  2.1915001E-09 -5.9105006E-11\n");
	}
	exit(0);
}
