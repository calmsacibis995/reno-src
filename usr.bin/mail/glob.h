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
 *
 *	@(#)glob.h	5.20 (Berkeley) 6/25/90
 */

/*
 * A bunch of global variable declarations lie herein.
 * def.h must be included first.
 */

int	msgCount;			/* Count of messages read in */
int	rcvmode;			/* True if receiving mail */
int	sawcom;				/* Set after first command */
char	*Tflag;				/* -T temp file for netnews */
int	senderr;			/* An error while checking */
int	edit;				/* Indicates editing a file */
int	readonly;			/* Will be unable to rewrite file */
int	noreset;			/* String resets suspended */
int	sourcing;			/* Currently reading variant file */
int	loading;			/* Loading user definitions */
int	cond;				/* Current state of conditional exc. */
FILE	*itf;				/* Input temp file buffer */
FILE	*otf;				/* Output temp file buffer */
int	image;				/* File descriptor for image of msg */
FILE	*input;				/* Current command input file */
char	mailname[PATHSIZE];		/* Name of current file */
char	prevfile[PATHSIZE];		/* Name of previous file */
char	*homedir;			/* Path name of home directory */
char	*myname;			/* My login name */
off_t	mailsize;			/* Size of system mailbox */
int	lexnumber;			/* Number of TNUMBER from scan() */
char	lexstring[STRINGLEN];		/* String from TSTRING, scan() */
int	regretp;			/* Pointer to TOS of regret tokens */
int	regretstack[REGDEP];		/* Stack of regretted tokens */
char	*string_stack[REGDEP];		/* Stack of regretted strings */
int	numberstack[REGDEP];		/* Stack of regretted numbers */
struct	message	*dot;			/* Pointer to current message */
struct	message	*message;		/* The actual message structure */
struct	var	*variables[HSHSIZE];	/* Pointer to active var list */
struct	grouphead	*groups[HSHSIZE];/* Pointer to active groups */
struct	ignoretab	ignore[2];	/* ignored and retained fields
					   0 is ignore, 1 is retain */
struct	ignoretab	saveignore[2];	/* ignored and retained fields
					   on save to folder */
struct	ignoretab	ignoreall[2];	/* special, ignore all headers */
char	**altnames;			/* List of alternate names for user */
int	debug;				/* Debug flag set */
int	screenwidth;			/* Screen width, or best guess */
int	screenheight;			/* Screen height, or best guess,
					   for "header" command */
int	realscreenheight;		/* the real screen height */

#include <setjmp.h>

jmp_buf	srbuf;


/*
 * The pointers for the string allocation routines,
 * there are NSPACE independent areas.
 * The first holds STRINGSIZE bytes, the next
 * twice as much, and so on.
 */

#define	NSPACE	25			/* Total number of string spaces */
struct strings {
	char	*s_topFree;		/* Beginning of this area */
	char	*s_nextFree;		/* Next alloctable place here */
	unsigned s_nleft;		/* Number of bytes left here */
} stringdope[NSPACE];
