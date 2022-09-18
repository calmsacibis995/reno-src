/*
 * $Id: misc-aix3.h,v 5.2 90/06/23 22:20:34 jsp Rel $
 *
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	@(#)misc-aix3.h	5.1 (Berkeley) 6/29/90
 */

struct ufs_args {
	char	*fspec;				/* Block device */
};

struct nfs_args {
	struct sockaddr_in	addr;		/* file server address */
	fhandle_t		fh;		/* File handle to be mounted */
	int			flags;		/* flags */
	int			wsize;		/* write size in bytes */
	int			rsize;		/* read size in bytes */
	int			timeo;		/* initial timeout in .1 secs */
	int			retrans;	/* times to retry send */
	char			*hostname;	/* server's hostname */
	int			acregmin;	/* attr cache file min secs */
	int			acregmax;	/* attr cache file max secs */
	int			acdirmin;	/* attr cache dir min secs */
	int			acdirmax;	/* attr cache dir max secs */
	char			*netname;	/* server's netname */
	int			biods;		/* number of BIODS */
};

/*
 * NFS mount option flags
 */
#define	MNTOPT_RO	"ro"	/* read only */
#define	MNTOPT_RW	"rw"	/* read/write */
#define	MNTOPT_SOFT	"soft"	/* soft mount */
#define	MNTOPT_HARD	"hard"	/* hard mount */
#define	MNTOPT_NOSUID	"nosuid"/* no set uid allowed */
#define	MNTOPT_NOAUTO	"noauto"/* hide entry from mount -a */
#define	MNTOPT_INTR	"intr"	/* allow interrupts on hard mount */
#define MNTOPT_SECURE 	"secure"/* use secure RPC for NFS */
#define MNTOPT_GRPID 	"grpid"	/* SysV-compatible group-id on create */
#define MNTOPT_NOSUB	"nosub"  /* disallow mounts beneath this one */
#define MNTOPT_MULTI	"multi"  /* Do multi-component lookup */
#define MNTOPT_NOAC 	"noac"   /* don't cache attributes */

#define NFSMNT_SOFT	0x001	/* soft mount (hard is default) */
#define NFSMNT_WSIZE	0x002	/* set write size */
#define NFSMNT_RSIZE	0x004	/* set read size */
#define NFSMNT_TIMEO	0x008	/* set initial timeout */
#define NFSMNT_RETRANS	0x010	/* set number of request retrys */
#define NFSMNT_HOSTNAME	0x020	/* set hostname for error printf */
#define NFSMNT_INT	0x040	/* allow interrupts on hard mount */
#define	NFSMNT_NOAC	0x080	/* don't cache attributes */
#define	NFSMNT_ACREGMIN	0x0100	/* set min secs for file attr cache */
#define	NFSMNT_ACREGMAX	0x0200	/* set max secs for file attr cache */
#define	NFSMNT_ACDIRMIN	0x0400	/* set min secs for dir attr cache */
#define	NFSMNT_ACDIRMAX	0x0800	/* set max secs for dir attr cache */
#define NFSMNT_SECURE	0x1000	/* secure mount */
#define NFSMNT_BIODS	0x10000	/* Number of biods for the file system */

#define DEF_BIODS	6
