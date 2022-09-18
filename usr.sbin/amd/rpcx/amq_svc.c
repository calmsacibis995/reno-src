/*
 * $Id: amq_svc.c,v 5.2 90/06/23 22:20:17 jsp Rel $
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
 *	@(#)amq_svc.c	5.1 (Berkeley) 7/19/90
 */

#include "am.h"
#include "amq.h"
extern bool_t xdr_amq_mount_info_qelem();

void
amq_program_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		amq_string amqproc_mnttree_1_arg;
		amq_string amqproc_umnt_1_arg;
		amq_setopt amqproc_setopt_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case AMQPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) amqproc_null_1;
		break;

	case AMQPROC_MNTTREE:
		xdr_argument = xdr_amq_string;
		xdr_result = xdr_amq_mount_tree_p;
		local = (char *(*)()) amqproc_mnttree_1;
		break;

	case AMQPROC_UMNT:
		xdr_argument = xdr_amq_string;
		xdr_result = xdr_void;
		local = (char *(*)()) amqproc_umnt_1;
		break;

	case AMQPROC_STATS:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_stats;
		local = (char *(*)()) amqproc_stats_1;
		break;

	case AMQPROC_EXPORT:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_tree_list;
		local = (char *(*)()) amqproc_export_1;
		break;

	case AMQPROC_SETOPT:
		xdr_argument = xdr_amq_setopt;
		xdr_result = xdr_int;
		local = (char *(*)()) amqproc_setopt_1;
		break;

	case AMQPROC_GETMNTFS:
		xdr_argument = xdr_void;
		xdr_result = xdr_amq_mount_info_qelem;
		local = (char *(*)()) amqproc_getmntfs_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		plog(XLOG_FATAL, "unable to free rpc arguments in amqprog_1");
		going_down(1);
	}
}

