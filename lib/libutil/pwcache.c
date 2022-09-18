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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)pwcache.c	5.4 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>

#define	NCACHE	64			/* power of 2 */
#define	MASK	NCACHE - 1		/* bits to store with */

static	int pwopen = 0;
static	int gropen = 0;

char *
user_from_uid(uid, nouser)
	uid_t uid;
	int nouser;
{
	static struct ncache {
		uid_t	uid;
		char	name[UT_NAMESIZE + 1];
	} c_uid[NCACHE];
	static char nbuf[15];		/* 32 bits == 10 digits */
	register struct passwd *pw;
	register struct ncache *cp;

	cp = c_uid + (uid & MASK);
	if (cp->uid != uid || !*cp->name) {
		if (pwopen == 0) {
			setpassent(1);
			pwopen++;
		}
		if (!(pw = getpwuid(uid))) {
			if (nouser)
				return((char *)NULL);
			(void)sprintf(nbuf, "%u", uid);
			return(nbuf);
		}
		cp->uid = uid;
		(void)strncpy(cp->name, pw->pw_name, UT_NAMESIZE);
		cp->name[UT_NAMESIZE] = '\0';
	}
	return(cp->name);
}

char *
group_from_gid(gid, nogroup)
	gid_t gid;
	int nogroup;
{
	static struct ncache {
		gid_t	gid;
		char	name[UT_NAMESIZE];
	} c_gid[NCACHE];
	static char nbuf[15];		/* 32 bits == 10 digits */
	register struct group *gr;
	register struct ncache *cp;

	cp = c_gid + (gid & MASK);
	if (cp->gid != gid || !*cp->name) {
		if (gropen == 0) {
			setgroupent(1);
			gropen++;
		}
		if (!(gr = getgrgid(gid))) {
			if (nogroup)
				return((char *)NULL);
			(void)sprintf(nbuf, "%u", gid);
			return(nbuf);
		}
		cp->gid = gid;
		(void)strncpy(cp->name, gr->gr_name, UT_NAMESIZE);
		cp->name[UT_NAMESIZE] = '\0';
	}
	return(cp->name);
}
