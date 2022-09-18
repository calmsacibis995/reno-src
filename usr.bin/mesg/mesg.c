/*
 * Copyright (c) 1987 Regents of the University of California.
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
"@(#) Copyright (c) 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mesg.c	4.6 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * mesg -- set current tty to accept or
 *	forbid write permission.
 *
 *	mesg [y] [n]
 *		y allow messages
 *		n forbid messages
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

static char *tty;

main(argc, argv)
	int argc;
	char **argv;
{
	struct stat sbuf;
	char *ttyname();

	if (!(tty = ttyname(2))) {
		fputs("mesg: not a device in /dev.\n", stderr);
		exit(-1);
	}
	if (stat(tty, &sbuf) < 0) {
		perror("mesg");
		exit(-1);
	}
	if (argc < 2) {
		if (sbuf.st_mode & 020) {
			fputs("is y\n", stderr);
			exit(0);
		}
		fputs("is n\n", stderr);
		exit(1);
	}
#define	OTHER_WRITE	020
	switch(*argv[1]) {
	case 'y':
		newmode(sbuf.st_mode | OTHER_WRITE);
		exit(0);
	case 'n':
		newmode(sbuf.st_mode &~ OTHER_WRITE);
		exit(1);
	default:
		fputs("usage: mesg [y] [n]\n", stderr);
		exit(-1);
	}
	/*NOTREACHED*/
}

static
newmode(m)
	u_short m;
{
	if (chmod(tty, m) < 0) {
		perror("mesg");
		exit(-1);
	}
}
