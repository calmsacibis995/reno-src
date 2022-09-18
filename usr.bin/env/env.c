/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)env.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <string.h>

main(argc, argv)
	int argc;
	char **argv;
{
	extern char **environ;
	extern int errno, optind;
	register char **ep, *p;
	char *cleanenv[1];
	int ch;

	while ((ch = getopt(argc, argv, "-")) != EOF)
		switch((char)ch) {
		case '-':
			environ = cleanenv;
			cleanenv[0] = NULL;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: env [-] [name=value ...] [command]\n");
			exit(1);
		}
	for (argv += optind; *argv && (p = index(*argv, '=')); ++argv)
		(void)setenv(*argv, ++p, 1);
	if (*argv) {
		execvp(*argv, argv);
		(void)fprintf(stderr, "env: %s: %s\n", *argv,
		    strerror(errno));
		exit(1);
	}
	for (ep = environ; *ep; ep++)
		(void)printf("%s\n", *ep);
	exit(0);
}
