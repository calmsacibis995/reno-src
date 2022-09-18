/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_srvcache.c	7.9 (Berkeley) 6/28/90
 */

/*
 * Reference: Chet Juszczak, "Improving the Performance and Correctness
 *            of an NFS Server", in Proc. Winter 1989 USENIX Conference,
 *            pages 53-63. San Diego, February 1989.
 */

#include "param.h"
#include "user.h"
#include "vnode.h"
#include "mount.h"
#include "kernel.h"
#include "systm.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "../netinet/in.h"
#include "nfsm_subs.h"
#include "nfsv2.h"
#include "nfsrvcache.h"
#include "nfs.h"

#if	((NFSRCHSZ&(NFSRCHSZ-1)) == 0)
#define	NFSRCHASH(xid)		(((xid)+((xid)>>16))&(NFSRCHSZ-1))
#else
#define	NFSRCHASH(xid)		(((unsigned)((xid)+((xid)>>16)))%NFSRCHSZ)
#endif

union rhead {
	union  rhead *rh_head[2];
	struct nfsrvcache *rh_chain[2];
} rhead[NFSRCHSZ];

static struct nfsrvcache nfsrvcachehead;
static struct nfsrvcache nfsrvcache[NFSRVCACHESIZ];

#define TRUE	1
#define	FALSE	0

/*
 * Static array that defines which nfs rpc's are nonidempotent
 */
int nonidempotent[NFS_NPROCS] = {
	FALSE,
	FALSE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
};

/* True iff the rpc reply is an nfs status ONLY! */
static int repliesstatus[NFS_NPROCS] = {
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	TRUE,
	FALSE,
	FALSE,
};

/*
 * Initialize the server request cache list
 */
nfsrv_initcache()
{
	register int i;
	register struct nfsrvcache *rp = nfsrvcache;
	register struct nfsrvcache *hp = &nfsrvcachehead;
	register union  rhead *rh = rhead;

	for (i = NFSRCHSZ; --i >= 0; rh++) {
		rh->rh_head[0] = rh;
		rh->rh_head[1] = rh;
	}
	hp->rc_next = hp->rc_prev = hp;
	for (i = NFSRVCACHESIZ; i-- > 0; ) {
		rp->rc_state = RC_UNUSED;
		rp->rc_flag = 0;
		rp->rc_forw = rp;
		rp->rc_back = rp;
		rp->rc_next = hp->rc_next;
		hp->rc_next->rc_prev = rp;
		rp->rc_prev = hp;
		hp->rc_next = rp;
		rp++;
	}
}

/*
 * Look for the request in the cache
 * If found then
 *    return action and optionally reply
 * else
 *    insert it in the cache
 *
 * The rules are as follows:
 * - if in progress, return DROP request
 * - if completed within DELAY of the current time, return DROP it
 * - if completed a longer time ago return REPLY if the reply was cached or
 *   return DOIT
 * Update/add new request at end of lru list
 */
nfsrv_getcache(nam, xid, proc, repp)
	struct mbuf *nam;
	u_long xid;
	int proc;
	struct mbuf **repp;
{
	register struct nfsrvcache *rp;
	register union  rhead *rh;
	struct mbuf *mb;
	caddr_t bpos;
	int ret;

	rh = &rhead[NFSRCHASH(xid)];
loop:
	for (rp = rh->rh_chain[0]; rp != (struct nfsrvcache *)rh; rp = rp->rc_forw) {
		if (xid == rp->rc_xid && proc == rp->rc_proc &&
		    nfs_netaddr_match(nam, &rp->rc_nam)) {
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			put_at_head(rp);
			if (rp->rc_state == RC_UNUSED)
				panic("nfsrv cache");
			if (rp->rc_state == RC_INPROG ||
			   (time.tv_sec - rp->rc_timestamp) < RC_DELAY) {
				nfsstats.srvcache_inproghits++;
				ret = RC_DROPIT;
			} else if (rp->rc_flag & RC_REPSTATUS) {
				nfsstats.srvcache_idemdonehits++;
				nfs_rephead(0, xid, rp->rc_status, repp, &mb,
					&bpos);
				rp->rc_timestamp = time.tv_sec;
				ret = RC_REPLY;
			} else if (rp->rc_flag & RC_REPMBUF) {
				nfsstats.srvcache_idemdonehits++;
				*repp = m_copym(rp->rc_reply, 0, M_COPYALL,
						M_WAIT);
				rp->rc_timestamp = time.tv_sec;
				ret = RC_REPLY;
			} else {
				nfsstats.srvcache_nonidemdonehits++;
				rp->rc_state = RC_INPROG;
				ret = RC_DOIT;
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup((caddr_t)rp);
			}
			return (ret);
		}
	}
	nfsstats.srvcache_misses++;
	rp = nfsrvcachehead.rc_prev;
	while ((rp->rc_flag & RC_LOCKED) != 0) {
		rp->rc_flag |= RC_WANTED;
		(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
	}
	remque(rp);
	put_at_head(rp);
	if (rp->rc_flag & RC_REPMBUF)
		mb = rp->rc_reply;
	else
		mb = (struct mbuf *)0;
	rp->rc_flag = 0;
	rp->rc_state = RC_INPROG;
	rp->rc_xid = xid;
	bcopy((caddr_t)nam, (caddr_t)&rp->rc_nam, sizeof (struct mbuf));
	rp->rc_proc = proc;
	insque(rp, rh);
	if (mb)
		m_freem(mb);
	return (RC_DOIT);
}

/*
 * Update a request cache entry after the rpc has been done
 */
nfsrv_updatecache(nam, xid, proc, repvalid, repstat, repmbuf)
	struct mbuf *nam;
	u_long xid;
	int proc;
	int repvalid;
	int repstat;
	struct mbuf *repmbuf;
{
	register struct nfsrvcache *rp;
	register union	rhead *rh;

	rh = &rhead[NFSRCHASH(xid)];
loop:
	for (rp = rh->rh_chain[0]; rp != (struct nfsrvcache *)rh; rp = rp->rc_forw) {
		if (xid == rp->rc_xid && proc == rp->rc_proc &&
		    nfs_netaddr_match(nam, &rp->rc_nam)) {
			if ((rp->rc_flag & RC_LOCKED) != 0) {
				rp->rc_flag |= RC_WANTED;
				(void) tsleep((caddr_t)rp, PZERO-1, "nfsrc", 0);
				goto loop;
			}
			rp->rc_flag |= RC_LOCKED;
			rp->rc_state = RC_DONE;
			/*
			 * If we have a valid reply update status and save
			 * the reply for non-idempotent rpc's.
			 * Otherwise invalidate entry by setting the timestamp
			 * to nil.
			 */
			if (repvalid) {
				rp->rc_timestamp = time.tv_sec;
				if (nonidempotent[proc]) {
					if (repliesstatus[proc]) {
						rp->rc_status = repstat;
						rp->rc_flag |= RC_REPSTATUS;
					} else {
						rp->rc_reply = m_copym(repmbuf,
							0, M_COPYALL, M_WAIT);
						rp->rc_flag |= RC_REPMBUF;
					}
				}
			} else {
				rp->rc_timestamp = 0;
			}
			rp->rc_flag &= ~RC_LOCKED;
			if (rp->rc_flag & RC_WANTED) {
				rp->rc_flag &= ~RC_WANTED;
				wakeup((caddr_t)rp);
			}
			return;
		}
	}
}
