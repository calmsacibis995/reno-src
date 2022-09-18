/*
 * Copyright (c) 1983 Regents of the University of California.
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
static char sccsid[] = "@(#)printlist.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include "gprof.h"

    /*
     *	these are the lists of names:
     *	there is the list head and then the listname
     *	is a pointer to the list head
     *	(for ease of passing to stringlist functions).
     */
struct stringlist	kfromhead = { 0 , 0 };
struct stringlist	*kfromlist = &kfromhead;
struct stringlist	ktohead = { 0 , 0 };
struct stringlist	*ktolist = &ktohead;
struct stringlist	fhead = { 0 , 0 };
struct stringlist	*flist = &fhead;
struct stringlist	Fhead = { 0 , 0 };
struct stringlist	*Flist = &Fhead;
struct stringlist	ehead = { 0 , 0 };
struct stringlist	*elist = &ehead;
struct stringlist	Ehead = { 0 , 0 };
struct stringlist	*Elist = &Ehead;

addlist( listp , funcname )
    struct stringlist	*listp;
    char		*funcname;
{
    struct stringlist	*slp;

    slp = (struct stringlist *) malloc( sizeof(struct stringlist));
    if ( slp == (struct stringlist *) 0 ) {
	fprintf( stderr, "gprof: ran out room for printlist\n" );
	done();
    }
    slp -> next = listp -> next;
    slp -> string = funcname;
    listp -> next = slp;
}

bool
onlist( listp , funcname )
    struct stringlist	*listp;
    char		*funcname;
{
    struct stringlist	*slp;

    for ( slp = listp -> next ; slp ; slp = slp -> next ) {
	if ( ! strcmp( slp -> string , funcname ) ) {
	    return TRUE;
	}
	if ( funcname[0] == '_' && ! strcmp( slp -> string , &funcname[1] ) ) {
	    return TRUE;
	}
    }
    return FALSE;
}
