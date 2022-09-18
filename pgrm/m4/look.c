/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit.
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
static char sccsid[] = "@(#)look.c	5.2 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * look.c
 * Facility: m4 macro processor
 * by: oz
 */

#include "mdef.h"
#include "extr.h"

extern char *strsave();

/*
 *  hash - compute hash value using the proverbial
 *	   hashing function. Taken from K&R.
 */
hash (name)
register char *name;
{
	register int h = 0;
	while (*name)
		h += *name++;
	return (h % HASHSIZE);
}

/*
 * lookup - find name in the hash table
 *
 */
ndptr lookup(name)
char *name;
{
	register ndptr p;

	for (p = hashtab[hash(name)]; p != nil; p = p->nxtptr)
		if (strcmp(name, p->name) == 0)
			break;
	return (p);
}

/*
 * addent - hash and create an entry in the hash
 *	    table. The new entry is added in front
 *	    of a hash bucket.
 */
ndptr addent(name)
char *name;
{
	register int h;
	ndptr p;

	h = hash(name);
	if ((p = (ndptr) malloc(sizeof(struct ndblock))) != NULL) {
		p->nxtptr = hashtab[h];
		hashtab[h] = p;
		p->name = strsave(name);
	}
	else
		error("m4: no more memory.");
	return p;
}

/*
 * remhash - remove an entry from the hashtable
 *
 */
remhash(name, all)
char *name;
int all;
{
	register int h;
	register ndptr xp, tp, mp;

	h = hash(name);
	mp = hashtab[h];
	tp = nil;
	while (mp != nil) {
		if (strcmp(mp->name, name) == 0) {
			mp = mp->nxtptr;
			if (tp == nil) {
				freent(hashtab[h]);
				hashtab[h] = mp;
			}
			else {
				xp = tp->nxtptr;
				tp->nxtptr = mp;
				freent(xp);
			}
			if (!all)
				break;
		}
		else {
			tp = mp;
			mp = mp->nxtptr;
		}
	}
}

/*
 * freent - free a hashtable information block
 *
 */
freent(p)
ndptr p;
{
	if (!(p->type & STATIC)) {
		free(p->name);
		if (p->defn != null)
			free(p->defn);
	}
	free(p);
}

