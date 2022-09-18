/* $Id: os-xinu43.h,v 5.2 90/06/23 22:21:01 jsp Rel $ */

/*
 * mt Xinu 4.3 (MORE/bsd) definitions for Amd (automounter)
 * Should work on both Vax and HP ...
 *
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	@(#)os-xinu43.h	5.1 (Berkeley) 6/29/90
 */

/*
 * Does the compiler grok void *
 */
#define	VOIDP

/*
 * Which version of the Sun RPC library we are using
 * This is the implementation release number, not
 * the protocol revision number.
 */
#define	RPC_4

/*
 * mt Xinu have a compatibility problem
 * with getreq vs. getreqset.  On SunOS
 * getreqset takes a pointer to an fd_set,
 * whereas on MORE/bsd, getreq takes a
 * fd_set directly (cf. an integer on SunOS).
 */
#define	svc_getreqset(p)	svc_getreq(*p)

/*
 * Which version of the NFS interface are we using.
 * This is the implementation release number, not
 * the protocol revision number.
 */
#define	NFS_4

/*
 * Name of filesystem types
 */
#define	MOUNT_TYPE_NFS	"nfs"
#define	MOUNT_TYPE_UFS	"ufs"
#undef MTAB_TYPE_UFS
#define	MTAB_TYPE_UFS	"ufs"

/*
 * Byte ordering
 */
#ifndef BYTE_ORDER
#include <machine/endian.h>
#endif /* BYTE_ORDER */

#undef ARCH_ENDIAN
#if BYTE_ORDER == LITTLE_ENDIAN
#define ARCH_ENDIAN "little"
#else
#if BYTE_ORDER == BIG_ENDIAN
#define ARCH_ENDIAN "big"
#else
XXX - Probably no hope of running Amd on this machine!
#endif /* BIG */
#endif /* LITTLE */

/*
 * Type of a file handle
 */
#undef NFS_FH_TYPE
#define NFS_FH_TYPE     caddr_t

/*
 * Type of filesystem type
 */
#undef MTYPE_TYPE
#define	MTYPE_TYPE	char *
