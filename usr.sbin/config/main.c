/*
 * Copyright (c) 1980 Regents of the University of California.
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
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	5.12 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <ctype.h>
#include "y.tab.h"
#include "config.h"

static char *PREFIX;

/*
 * Config builds a set of files for building a UNIX
 * system given a description of the desired system.
 */
main(argc, argv)
	int argc;
	char **argv;
{

	extern char *optarg;
	extern int optind;
	struct stat buf;
	int ch;
	char *p;

	while ((ch = getopt(argc, argv, "gp")) != EOF)
		switch((char)ch) {
		case 'g':
			debugging++;
			break;
		case 'p':
			profiling++;
			break;
		case '?':
		default:
			goto usage;
		}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
usage:		fputs("usage: config [-gp] sysname\n", stderr);
		exit(1);
	}

	if (freopen(PREFIX = *argv, "r", stdin) == NULL) {
		perror(PREFIX);
		exit(2);
	}
	if (stat(p = path((char *)NULL), &buf)) {
		if (mkdir(p, 0777)) {
			perror(p);
			exit(2);
		}
	}
	else if ((buf.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "config: %s isn't a directory.\n", p);
		exit(2);
	}

	dtab = NULL;
	confp = &conf_list;
	if (yyparse())
		exit(3);
	switch (machine) {

	case MACHINE_VAX:
		vax_ioconf();		/* Print ioconf.c */
		ubglue();		/* Create ubglue.s */
		break;

	case MACHINE_TAHOE:
		tahoe_ioconf();
		vbglue();
		break;

	case MACHINE_HP300:
		hp300_ioconf();
		hpglue();
		break;

	default:
		printf("Specify machine type, e.g. ``machine vax''\n");
		exit(1);
	}
	/*
	 * make symbolic links in compilation directory
	 * for "sys" (to make genassym.c work along with #include <sys/xxx>)
	 * and similarly for "machine".
	 */
	{
	char xxx[80];

	(void) symlink("../sys", path("sys"));
	(void) sprintf(xxx, "../%s", machinename);
	(void) symlink(xxx, path("machine"));
	}
	makefile();			/* build Makefile */
	headers();			/* make a lot of .h files */
	swapconf();			/* swap config files */
	printf("Don't forget to run \"make depend\"\n");
	exit(0);
}

/*
 * get_word
 *	returns EOF on end of file
 *	NULL on end of line
 *	pointer to the word otherwise
 */
char *
get_word(fp)
	register FILE *fp;
{
	static char line[80];
	register int ch;
	register char *cp;

	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return ((char *)EOF);
	if (ch == '\n')
		return (NULL);
	cp = line;
	*cp++ = ch;
	while ((ch = getc(fp)) != EOF) {
		if (isspace(ch))
			break;
		*cp++ = ch;
	}
	*cp = 0;
	if (ch == EOF)
		return ((char *)EOF);
	(void) ungetc(ch, fp);
	return (line);
}

/*
 * prepend the path to a filename
 */
char *
path(file)
	char *file;
{
	register char *cp;

	cp = malloc((unsigned)(strlen(PREFIX)+strlen(file)+5));
	(void) strcpy(cp, "../");
	(void) strcat(cp, PREFIX);
	if (file) {
		(void) strcat(cp, "/");
		(void) strcat(cp, file);
	}
	return (cp);
}
