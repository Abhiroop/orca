#include	"param.h"	/* for CHECK */

main()	{
	extern char *malloc();
	char *addr1;
	char *addr7;
	char *addr8;
	char *addr14;
	char *addr15;
	char *addr16;
	char *addr17;
	
	printf("malloc(1) = %ld\n", (long)(addr1 = malloc(1)));
	printf("malloc(7) = %ld\n", (long)(addr7 = malloc(7)));
	printf("malloc(8) = %ld\n", (long)(addr8 = malloc(8)));
	printf("malloc(14) = %ld\n", (long)(addr14 = malloc(14)));
	printf("malloc(15) = %ld\n", (long)(addr15 = malloc(15)));
	printf("malloc(16) = %ld\n", (long)(addr16 = malloc(16)));
	printf("malloc(17) = %ld\n", (long)(addr17 = malloc(17)));
	maldump(0);
	free(addr1);
	free(addr7);
	free(addr8);
	free(addr14);
	free(addr15);
	free(addr16);
	free(addr17);
	maldump(0);
}
