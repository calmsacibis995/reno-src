/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * from: Utah $Hdr: rmp_proto.c 1.3 89/06/07$
 *
 *	@(#)rmp_proto.c	7.1 (Berkeley) 5/8/90
 */

#include "param.h"
#include "socket.h"
#include "protosw.h"
#include "domain.h"

#include "rmp.h"

#ifdef RMP
/*
 * HP Remote Maintenance Protocol (RMP) family: BOOT
 */

extern	struct domain	rmpdomain;
extern	int		raw_usrreq(), rmp_output();

struct protosw rmpsw[] = {
  {	SOCK_RAW,	&rmpdomain,	RMPPROTO_BOOT,	PR_ATOMIC|PR_ADDR,
	0,		rmp_output,	0,		0,
	raw_usrreq,
	0,		0,		0,		0,
  },
};

struct domain rmpdomain = {
	AF_RMP, "RMP", 0, 0, 0, rmpsw, &rmpsw[sizeof(rmpsw)/sizeof(rmpsw[0])]
};

#endif
