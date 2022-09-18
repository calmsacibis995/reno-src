/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *
 *	@(#)config.h	5.9 (Berkeley) 6/1/90
 */

#define	DEFSHELL	1			/* Bourne shell */

/*
 * DEFMAXJOBS
 * DEFMAXLOCAL
 *	These control the default concurrency. On no occasion will more
 *	than DEFMAXJOBS targets be created at once (locally or remotely)
 *	DEFMAXLOCAL is the highest number of targets which will be
 *	created on the local machine at once. Note that if you set this
 *	to 0, nothing will ever happen...
 */
#define DEFMAXJOBS	4
#define DEFMAXLOCAL	1

/*
 * INCLUDES
 * LIBRARIES
 *	These control the handling of the .INCLUDES and .LIBS variables.
 *	If INCLUDES is defined, the .INCLUDES variable will be filled
 *	from the search paths of those suffixes which are marked by
 *	.INCLUDES dependency lines. Similarly for LIBRARIES and .LIBS
 *	See suff.c for more details.
 */
#define INCLUDES
#define LIBRARIES

/*
 * LIBSUFF
 *	Is the suffix used to denote libraries and is used by the Suff module
 *	to find the search path on which to seek any -l<xx> targets.
 *
 * RECHECK
 *	If defined, Make_Update will check a target for its current
 *	modification time after it has been re-made, setting it to the
 *	starting time of the make only if the target still doesn't exist.
 *	Unfortunately, under NFS the modification time often doesn't
 *	get updated in time, so a target will appear to not have been
 *	re-made, causing later targets to appear up-to-date. On systems
 *	that don't have this problem, you should defined this. Under
 *	NFS you probably should not, unless you aren't exporting jobs.
 *
 * POSIX
 *	If the POSIX standard for Make is to be followed. There are
 *	several areas that I dislike, hence this constant.
 */
#define	LIBSUFF	".a"
#define	RECHECK
/*#define POSIX*/
