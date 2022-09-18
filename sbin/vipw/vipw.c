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
static char sccsid[] = "@(#)vipw.c	5.14 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

char *passwd, *temp;

main()
{
	extern int errno;
	register int n, fd_passwd, fd;
	struct rlimit rlim;
	struct stat s1, s2;
	FILE *tfp;
	char *fend, *tend;
	char buf[8*1024], from[MAXPATHLEN], to[MAXPATHLEN];

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);

	(void)umask(0);

	temp = _PATH_PTMP;
	if ((fd = open(temp, O_RDWR|O_CREAT|O_EXCL, 0600)) < 0) {
		if (errno == EEXIST)
			(void)fprintf(stderr, "vipw: password file busy.\n");
		else
			(void)fprintf(stderr,
			    "vipw: %s: %s\n", temp, strerror(errno));
		exit(1);
	}
	passwd = _PATH_MASTERPASSWD;
	if ((fd_passwd = open(passwd, O_RDONLY, 0)) < 0) {
		(void)fprintf(stderr, "vipw: %s: %s\n", passwd,
		    strerror(errno));
		exit(1);
	}
	while ((n = read(fd_passwd, buf, sizeof(buf))) > 0)
		if (write(fd, buf, n) != n)
			goto syserr;

	if (n == -1 || close(fd_passwd)) {
syserr:		(void)fprintf(stderr, "vipw: %s: %s; ",
		    passwd, strerror(errno));
		stop(1);
	}

	(void)fstat(fd, &s1);
	(void)close(fd);
	for (;;) {
		if (edit()) {
			(void)fprintf(stderr, "vipw: edit failed; ");
			stop(1);
		}
		/*
		 * close and re-open the file each time we edit it; some
		 * editors create a new physical file on each edit session.
		 */
		if (!(tfp = fopen(temp, "r"))) {
			(void)fprintf(stderr, "vipw: %s: %s; ",
			    temp, strerror(errno));
			stop(1);
		}
		(void)fstat(fileno(tfp), &s2);
		if (s1.st_mtime == s2.st_mtime) {
			(void)fprintf(stderr, "vipw: no changes made; ");
			stop(0);
		}
		if (!check(tfp))
			break;
		if (prompt())
			stop(0);
		(void)fstat(fileno(tfp), &s1);
		(void)fclose(tfp);
	}

	switch(fork()) {
	case 0:
		break;
	case -1:
		(void)fprintf(stderr, "vipw: can't fork; ");
		stop(1);
		/* NOTREACHED */
	default:
		exit(0);
		/* NOTREACHED */
	}

	if (makedb(temp)) {
		(void)fprintf(stderr, "vipw: mkpasswd failed; ");
		stop(1);
	}

	/*
	 * possible race; have to rename four files, and someone could slip
	 * in between them.  LOCK_EX and rename the ``passwd.dir'' file first
	 * so that getpwent(3) can't slip in; the lock should never fail and
	 * it's unclear what to do if it does.  Rename ``ptmp'' last so that
	 * passwd/vipw/chpass can't slip in.
	 */
	(void)setpriority(PRIO_PROCESS, 0, -20);
	fend = strcpy(from, temp) + strlen(temp);
	tend = strcpy(to, _PATH_PASSWD) + strlen(_PATH_PASSWD);
	bcopy(".dir", fend, 5);
	bcopy(".dir", tend, 5);
	if ((fd = open(from, O_RDONLY, 0)) >= 0)
		(void)flock(fd, LOCK_EX);
	/* here we go... */
	(void)rename(from, to);
	bcopy(".pag", fend, 5);
	bcopy(".pag", tend, 5);
	(void)rename(from, to);
	bcopy(".orig", fend, 6);
	(void)rename(from, _PATH_PASSWD);
	(void)rename(temp, passwd);
	/* done! */
	exit(0);
}

check(tfp)
	FILE *tfp;
{
	register long id;
	register int lcnt, root;
	register char *p, *sh;
	long atol();
	char buf[1024], *bp, *getusershell();

	for (lcnt = 1; fgets(buf, sizeof(buf), tfp); ++lcnt) {
		/* skip lines that are too big */
		if (!(p = index(buf, '\n'))) {
			(void)fprintf(stderr, "vipw: line too long");
			goto bad;
		}
		*p = '\0';
		bp = buf;
		if (!(p = strsep(&bp, ":")))		/* login */
			goto general;
		root = !strcmp(p, "root");
		(void)strsep(&bp, ":");	/* passwd */
		if (!(p = strsep(&bp, ":")))	/* uid */
			goto general;
		id = atol(p);
		if (root && id) {
			(void)fprintf(stderr, "vipw: root uid should be 0");
			goto bad;
		}
		if (id > USHRT_MAX) {
			(void)fprintf(stderr, "vipw: %s > max uid value (%d)",
			    p, USHRT_MAX);
			goto bad;
		}
		if (!(p = strsep(&bp, ":")))	/* gid */
			goto general;
		id = atol(p);
		if (id > USHRT_MAX) {
			(void)fprintf(stderr, "vipw: %s > max gid value (%d)",
			    p, USHRT_MAX);
			goto bad;
		}
		(void)strsep(&bp, ":");	/* class */
		(void)strsep(&bp, ":");	/* change */
		(void)strsep(&bp, ":");	/* expire */
		(void)strsep(&bp, ":");	/* gecos */
		(void)strsep(&bp, ":");	/* directory */
		if (!(p = strsep(&bp, ":")))	/* shell */
			goto general;
		if (root && *p)				/* empty == /bin/sh */
			for (setusershell();;)
				if (!(sh = getusershell())) {
					(void)fprintf(stderr,
					    "vipw: warning, unknown root shell.\n");
					break;
				}
				else if (!strcmp(p, sh))
					break;
		if (p = strsep(&bp, ":")) {	/* too many */
(void)fprintf(stderr, "got {%s}\n", p);
general:		(void)fprintf(stderr, "vipw: corrupted entry");
bad:			(void)fprintf(stderr, "; line #%d.\n", lcnt);
			(void)fflush(stderr);
			return(1);
		}
	}
	return(0);
}

makedb(file)
	char *file;
{
	int status, pid, w;

	if (!(pid = vfork())) {
		execl(_PATH_MKPASSWD, "mkpasswd", "-p", file, NULL);
		_exit(127);
	}
	while ((w = wait(&status)) != pid && w != -1);
	return(w == -1 || status);
}

edit()
{
	extern int errno;
	int status, pid, w;
	char *p, *editor, *getenv(), *strerror();

	if (editor = getenv("EDITOR")) {
		if (p = rindex(editor, '/'))
			++p;
		else
			p = editor;
	}
	else
		p = editor = "vi";
	if (!(pid = vfork())) {
		execlp(editor, p, temp, NULL);
		(void)fprintf(stderr, "vipw: %s: %s\n", editor,
		    strerror(errno));
		_exit(127);
	}
	while ((w = wait(&status)) != pid && w != -1);
	return(w == -1 || status);
}

prompt()
{
	register int c;

	for (;;) {
		(void)printf("re-edit the password file? [y]: ");
		(void)fflush(stdout);
		c = getchar();
		if (c != EOF && c != (int)'\n')
			while (getchar() != (int)'\n');
		return(c == (int)'n');
	}
	/* NOTREACHED */
}

stop(val)
	int val;
{
	(void)fprintf(stderr, "%s unchanged.\n", passwd);
	(void)unlink(temp);
	exit(val);
}
