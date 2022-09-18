/*
 * Copyright (c) 1981, 1988 The Regents of the University of California.
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
"@(#) Copyright (c) 1981, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rmail.c	4.15 (Berkeley) 5/31/90";
#endif /* not lint */

/*
 * RMAIL -- UUCP mail server.
 *
 *	This program reads the >From ... remote from ... lines that
 *	UUCP is so fond of and turns them into something reasonable.
 *	It calls sendmail giving it a -f option built from these lines. 
 */

#include <sysexits.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <paths.h>

typedef char bool;
#define TRUE	1
#define FALSE	0

extern char *index();
extern char *rindex();

char *Domain = "UUCP";		/* Default "Domain" */

main(argc, argv)
	int argc;
	char **argv;
{
	char lbuf[1024];	/* one line of the message */
	char from[512];		/* accumulated path of sender */
	char ufrom[512];	/* user on remote system */
	char sys[512];		/* a system in path */
	char fsys[512];		/* first system in path */
	char junk[1024];	/* scratchpad */
	char *args[100];	/* arguments to mailer command */
	register char *cp;
	register char *uf = NULL;	/* ptr into ufrom */
	int i;
	long position;
	struct stat sbuf;
#ifdef DEBUG
	bool Debug;

	if (argc > 1 && strcmp(argv[1], "-T") == 0) {
		Debug = TRUE;
		argc--;
		argv++;
	}
#endif

	if (argc < 2) {
		fprintf(stderr, "Usage: rmail user ...\n");
		exit(EX_USAGE);
	}
	if (argc > 2 && strncmp(argv[1], "-D", 2) == 0) {
		Domain = &argv[1][2];
		argc -= 2;
		argv += 2;
	}
	from[0] = '\0';
	fsys[0] = '\0';
	(void) strcpy(ufrom, _PATH_DEVNULL);

	for (position = 0;; position = ftell(stdin)) {
		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			exit(EX_DATAERR);
		if (strncmp(lbuf, "From ", 5) != 0 &&
		    strncmp(lbuf, ">From ", 6) != 0)
			break;
		(void) sscanf(lbuf, "%s %s", junk, ufrom);
		cp = lbuf;
		uf = ufrom;
		for (;;) {
			cp = index(cp + 1, 'r');
			if (cp == NULL) {
				register char *p = rindex(uf, '!');

				if (p != NULL) {
					*p = '\0';
					(void) strcpy(sys, uf);
					uf = p + 1;
					break;
				}
				(void) strcpy(sys, "");
				break;	/* no "remote from" found */
			}
#ifdef DEBUG
			if (Debug)
				printf("cp='%s'\n", cp);
#endif
			if (strncmp(cp, "remote from ", 12) == 0)
				break;
		}
		if (cp != NULL)
			(void) sscanf(cp, "remote from %s", sys);
		if (fsys[0] == '\0')
			(void) strcpy(fsys, sys);
		if (sys[0]) {
			(void) strcat(from, sys);
			(void) strcat(from, "!");
		}
#ifdef DEBUG
		if (Debug)
			printf("ufrom='%s', sys='%s', from now '%s'\n", uf, sys, from);
#endif
	}
	if (uf == NULL) {	/* No From line was provided */
		fprintf(stderr, "No From line in rmail\n");
		exit(EX_DATAERR);
	}
	(void) strcat(from, uf);
	(void) fstat(0, &sbuf);
	(void) lseek(0, position, L_SET);

	/*
	 * Now we rebuild the argument list and chain to sendmail. Note that
	 * the above lseek might fail on irregular files, but we check for
	 * that case below. 
	 */
	i = 0;
	args[i++] = _PATH_SENDMAIL;
	args[i++] = "-oee";		/* no errors, just status */
	args[i++] = "-odq";		/* queue it, don't try to deliver */
	args[i++] = "-oi";		/* ignore '.' on a line by itself */
	if (fsys[0] != '\0') {		/* set sender's host name */
		static char junk2[512];

		if (index(fsys, '.') == NULL) {
			(void) strcat(fsys, ".");
			(void) strcat(fsys, Domain);
		}
		(void) sprintf(junk2, "-oMs%s", fsys);
		args[i++] = junk2;
	}
					/* set protocol used */
	(void) sprintf(junk, "-oMr%s", Domain);
	args[i++] = junk;
	if (from[0] != '\0') {		/* set name of ``from'' person */
		static char junk2[512];

		(void) sprintf(junk2, "-f%s", from);
		args[i++] = junk2;
	}
	for (; *++argv != NULL; i++) {
		/*
		 * don't copy arguments beginning with - as they will
		 * be passed to sendmail and could be interpreted as flags
		 * should be fixed in sendmail by using getopt(3), and
		 * just passing "--" before regular args.
		 */
		if (**argv != '-')
			args[i] = *argv;
	}
	args[i] = NULL;
#ifdef DEBUG
	if (Debug) {
		printf("Command:");
		for (i = 0; args[i]; i++)
			printf(" %s", args[i]);
		printf("\n");
	}
#endif
	if ((sbuf.st_mode & S_IFMT) != S_IFREG) {
		/*
		 * If we were not called with standard input on a regular
		 * file, then we have to fork another process to send the
		 * first line down the pipe. 
		 */
		int pipefd[2];
#ifdef DEBUG
		if (Debug)
			printf("Not a regular file!\n");
#endif
		if (pipe(pipefd) < 0)
			exit(EX_OSERR);
		if (fork() == 0) {
			/*
			 * Child: send the message down the pipe. 
			 */
			FILE *out;

			out = fdopen(pipefd[1], "w");
			close(pipefd[0]);
			fputs(lbuf, out);
			while (fgets(lbuf, sizeof lbuf, stdin))
				fputs(lbuf, out);
			(void) fclose(out);
			exit(EX_OK);
		}
		/*
		 * Parent: call sendmail with pipe as standard input 
		 */
		close(pipefd[1]);
		dup2(pipefd[0], 0);
	}
	execv(_PATH_SENDMAIL, args);
	fprintf(stderr, "Exec of %s failed!\n", _PATH_SENDMAIL);
	exit(EX_OSERR);
}
