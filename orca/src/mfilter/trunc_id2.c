/* Hack for making identifiers shorter */

#include <stdio.h>
#include <ctype.h>

#define MAX_ID_LEN	1024
#define TRUNC_LEN	36

char *ProgName;
int trunc_len = TRUNC_LEN;

DoOption(s)
	char	*s;
{
	if (isdigit(s[1])){
		trunc_len = atoi(&s[1]);
	}
}

main(argc, argv)
	char *argv[];
{
	char **nargv;
	int nargc = 0;
	FILE *fp;

	ProgName = *argv++;
	nargv = argv;

	while (--argc > 0) {
		if ((*argv)[0] == '-') {
			DoOption(*argv++);
		}
		else {
			nargv[nargc++] = *argv++;
		}
	}

	if (nargc > 0) {
		while (nargc-- > 0) {
			if ((fp = fopen(*nargv, "r")) == NULL) {
				fprintf(stderr, "%s: cannot read file \"%s\"\n",
					ProgName, *nargv);
			}
			else {
				DoFile(fp);
			}
			nargv++;
		}
	}
	else {
		DoFile(stdin);
	}
	exit(0);
}

DoFile(fp)
	FILE *fp;
{
	register c;

	while ((c = getc(fp)) != EOF) {
		switch (c) {

		case '"':
		case '\'':
			putchar(c);
			SkipString(fp, c);
			break;
		
		case '/':
			putchar(c);
			if ((c = getc(fp)) == '*') {
				putchar(c);
				SkipComment(fp);
			}
			else {
				ungetc(c, fp);
			}
			break;

		default:
			if (isalpha(c) || c == '_') {
				DoIdent(fp, c);
			}
			else if (isdigit(c)) {
				putchar(c);
				DoNum(fp, c);
			}
			else putchar(c);
			break;
		}
	}
	fclose(fp);
}

SkipString(fp, stopc)
	FILE *fp;
{
	register c;

	while ((c = getc(fp)) != EOF) {
		putchar(c);
		if (c == stopc) {
			return;
		}

		if (c == '\\') {
			c = getc(fp);
			putchar(c);
		}
	}
}

SkipComment(fp)
	FILE *fp;
{
	register c;

	while ((c = getc(fp)) != EOF) {
		putchar(c);
		if (c == '*') {
			if ((c = getc(fp)) == '/') {
				putchar(c);
				return;
			}
			ungetc(c, fp);
		}
	}
}

DoIdent(fp, s)
	FILE *fp;
{
	char id_buf[MAX_ID_LEN];
	register cnt = 1;
	register c;

	id_buf[0] = s;

	while ((c = getc(fp)) != EOF) {
		if (isalnum(c) || c == '_') {
			id_buf[cnt++] = c;
		}
		else {
			ungetc(c, fp);
			id_buf[cnt] = '\0';
			if (cnt > trunc_len) {
				static int warning_given;

				if (! warning_given) {
				    fprintf(stderr, "%s: truncation performed\n",
					ProgName);

				    warning_given = 1;
				}
				/*
				putchar(id_buf[0]);
				fputs(&id_buf[cnt-trunc_len], stdout);
            */
				id_buf[ 10 ] = '\0';
				fputs(&id_buf[0], stdout);
				fputs(&id_buf[cnt-trunc_len+10], stdout);
			}
			else {
				fputs(id_buf, stdout);
			}
			return;
		}
	}
}


#define inrange(c, l, u)	((unsigned)((c) - (l)) <= ((u) - (l)))
#define isdec(c) inrange(c, '0', '9')
#define isoct(c) inrange(c, '0', '7')
#define ishex(c) (isdec(c) || inrange(c, 'a', 'f') || inrange(c, 'A', 'F'))
#define getdec(c, fp)	do { c = getc((fp)); putchar(c);} while (isdec(c))
#define getoct(c, fp)	do { c = getc((fp)); putchar(c);} while (isoct(c))
#define gethex(c, fp)	do { c = getc((fp)); putchar(c);} while (ishex(c))

DoNum(fp, c)
        register FILE *fp;
{
        if (c != '0') {
                getdec(c, fp);
                if (c == '.')
                        getdec(c, fp);
                if (c == 'e') {
                        c = getc(fp);
                        putchar(c);
                        if (c == '+' || c == '-') {
                                c = getc(fp);
                                putchar(c);
                        }
                        if (isdec(c))
                                getdec(c, fp);
                }
        }
        else {
                c = getc(fp);
                putchar(c);
                if (c == 'x' || c == 'X')
                        gethex(c, fp);
                else
                if (isoct(c))
                        getoct(c, fp);
        }
}

