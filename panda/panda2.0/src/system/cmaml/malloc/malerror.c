extern char *malloc(), *realloc();

void
main(int argc, char **argv) {
	char *ar;
	int i;

	ar = malloc(10000);
	*ar = '\0';
	free(ar);

	ar = malloc(10000);
	for (i = 0; i < 24; i++) {
		printf("%d, ", ar[-i]);
	}
	*(ar-9) = 77;
	free(ar);

	exit(0);
}

