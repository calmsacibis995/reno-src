/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
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
"@(#) Copyright (c) 1980, 1986 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)halt.c	5.7 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Halt
 */
#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/signal.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <paths.h>

main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	register int qflag = 0;
	struct passwd *pw, *getpwuid();
	int ch, howto, needlog = 1;
	char *user, *ttyn, *getlogin(), *ttyname();

	howto = RB_HALT;
	ttyn = ttyname(2);
	while ((ch = getopt(argc, argv, "lnqy")) != EOF)
		switch((char)ch) {
		case 'l':		/* undocumented; for shutdown(8) */
			needlog = 0;
			break;
		case 'n':
			howto |= RB_NOSYNC;
			break;
		case 'q':
			qflag++;
			break;
		case 'y':
			ttyn = 0;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: halt [-nqy]\n");
			exit(1);
		}

	if (ttyn && ttyn[sizeof(_PATH_TTY) - 1] == 'd') {
		fprintf(stderr, "halt: dangerous on a dialup; use ``halt -y'' if you are really sure\n");
		exit(1);
	}

	if (needlog) {
		openlog("halt", 0, LOG_AUTH);
		if ((user = getlogin()) == NULL)
			if ((pw = getpwuid(getuid())))
				user = pw->pw_name;
			else
				user = "???";
		syslog(LOG_CRIT, "halted by %s", user);
	}

	signal(SIGHUP, SIG_IGN);		/* for network connections */
	if (kill(1, SIGTSTP) == -1) {
		fprintf(stderr, "halt: can't idle init\n");
		exit(1);
	}
	sleep(1);
	(void) kill(-1, SIGTERM);	/* one chance to catch it */
	sleep(5);

	if (!qflag) for (i = 1; ; i++) {
		if (kill(-1, SIGKILL) == -1) {
			extern int errno;

			if (errno == ESRCH)
				break;

			perror("halt: kill");
			kill(1, SIGHUP);
			exit(1);
		}
		if (i > 5) {
			fprintf(stderr,
			    "CAUTION: some process(es) wouldn't die\n");
			break;
		}
		setalarm(2 * i);
		pause();
	}

	if (!qflag && (howto & RB_NOSYNC) == 0) {
		logwtmp("~", "shutdown", "");
		sync();
		setalarm(5);
		pause();
	}
	syscall(55, howto);
	perror("halt");
}

dingdong()
{
	/* RRRIIINNNGGG RRRIIINNNGGG */
}

setalarm(n)
{
	signal(SIGALRM, dingdong);
	alarm(n);
}
