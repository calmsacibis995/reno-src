/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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
"@(#) Copyright (c) 1983, 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)implogd.c	5.9 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/file.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netimp/if_imp.h>

#include <sgtty.h>
#include "pathnames.h"

/*
 * Socket address, internet style, with
 * unused space taken by timestamp and packet
 * size.
 */
struct sockstamp {
	short	sin_family;
	u_short	sin_port;
	struct	in_addr sin_addr;
	time_t	sin_time;
	int	sin_len;
};

main()
{
	register int len, log, s;
	struct sockstamp from;
	int fromlen;
	u_char request[1024];
	time_t time();

	openlog("implogd", LOG_PID|LOG_ODELAY|LOG_PERROR, LOG_DAEMON);
	log = open(_PATH_IMPLOG, O_CREAT|O_WRONLY|O_APPEND, 0644);
	if (log < 0) {
		syslog(LOG_ERR, "%s: %m\n", _PATH_IMPLOG);
		exit(1);
	}
	from.sin_time = time((time_t *)NULL);
	from.sin_len = sizeof(time_t);
	(void)write(log, (char *)&from, sizeof(from));
	if ((s = socket(AF_IMPLINK, SOCK_RAW, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m\n");
		exit(1);
	}
#ifndef DEBUG
	{
		register int i, tt;

		if (fork())
			exit(0);
		for (i = 0; i < 10; i++)
			if (i != log && i != s)
				(void) close(i);
		(void) open("/", O_RDONLY, 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		tt = open(_PATH_TTY, O_RDWR, 0);
		if (tt > 0) {
			ioctl(tt, TIOCNOTTY, 0);
			(void)close(tt);
		}
	}
#endif
	for (fromlen = sizeof(from);;) {
		len = recvfrom(s, request, sizeof(request), 0,
		    &from, &fromlen);
		if (len < 0) {
			syslog(LOG_ERR, "recvfrom: %m\n");
			continue;
		}
		if (len == 0 || len > IMPMTU)		/* sanity */
			continue;
		from.sin_len = len;
		from.sin_time = time((time_t *)NULL);
		(void)write(log, (char *)&from, sizeof(from));
		(void)write(log, request, len);
	}
	/*NOTREACHED*/
}
