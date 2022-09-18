/*
 * Copyright (c) 1989 The Regents of the University of California.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)nice.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <ctype.h>

#define	DEFNICE	10

/* ARGSUSED */
main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno;
	int niceness;
	char *strerror();

	niceness = DEFNICE;
	if (argv[1][0] == '-')
		if (isdigit(argv[1][1])) {
			niceness = atoi(argv[1] + 1);
			++argv;
		}
		else {
			(void)fprintf(stderr, "nice: illegal option -- %c\n",
			    argv[1][1]);
			usage();
		}

	if (!argv[1])
		usage();

	errno = 0;
	niceness += getpriority(PRIO_PROCESS, 0);
	if (errno) {
		(void)fprintf(stderr, "nice: getpriority: %s\n",
		    strerror(errno));
		exit(1);
	}
	if (setpriority(PRIO_PROCESS, 0, niceness)) {
		(void)fprintf(stderr,
		    "nice: setpriority: %s\n", strerror(errno));
		exit(1);
	}
	execvp(argv[1], &argv[1]);
	(void)fprintf(stderr,
	    "nice: %s: %s\n", argv[1], strerror(errno));
	exit(1);
}

usage()
{
	(void)fprintf(stderr,
	    "nice [ -# ] command [ options ] [ operands ]\n");
	exit(1);
}
