/*-
 * Copyright (c) 1980, 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tput.c	5.7 (Berkeley) 6/7/90";
#endif /* not lint */

#include <sys/termios.h>
#include <stdio.h>
#include <unistd.h>

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	int ch, exitval, n, outc();
	char *cptr, *p, *term, buf[1024], tbuf[1024];
	char *getenv(), *tgetstr(), *realname();

	term = NULL;
	while ((ch = getopt(argc, argv, "T:")) != EOF)
		switch(ch) {
		case 'T':
			term = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!term && !(term = getenv("TERM"))) {
		(void)fprintf(stderr, "tput: no terminal type specified.\n");
		exit(2);
	}
	if (tgetent(tbuf, term) != 1) {
		(void)fprintf(stderr, "tput: tgetent failure.\n");
		exit(2);
	}
	setospeed();
	for (cptr = buf, exitval = 0; p = *argv; ++argv) {
		switch(*p) {
		case 'c':
			if (!strcmp(p, "clear"))
				p = "cl";
			break;
		case 'i':
			if (!strcmp(p, "init"))
				p = "is";
			break;
		case 'l':
			if (!strcmp(p, "longname"))
				prlongname(tbuf);
			continue;
		case 'r':
			if (!strcmp(p, "reset"))
				p = "rs";
			break;
		}
		if (tgetstr(p, &cptr))
			(void)tputs(buf, 1, outc);
		else if ((n = tgetnum(p)) != -1)
			(void)printf("%d\n", n);
		else
			exitval = !tgetflag(p);
	}
	exit(exitval);
}

prlongname(buf)
	char *buf;
{
	register char *p;
	int savech;
	char *savep;

	for (p = buf; *p && *p != ':'; ++p);
	savech = *(savep = p);
	for (*p = '\0'; p >= buf && *p != '|'; --p);
	(void)printf("%s\n", p + 1);
	*savep = savech;
}

setospeed()
{
	extern int errno, ospeed;
	struct termios t;
	char *strerror();

	if (tcgetattr(STDOUT_FILENO, &t) != -1)
		ospeed = 0;
	else
		ospeed = cfgetospeed(&t);
}

outc(c)
	int c;
{
	putchar(c);
}

usage()
{
	(void)fprintf(stderr, "usage: tput [-T term] attribute ...\n");
	exit(1);
}
