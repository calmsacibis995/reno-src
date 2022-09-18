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
 *	@(#)nfs_syscalls.c	7.18 (Berkeley) 6/28/90
 */

#include "param.h"
#include "systm.h"
#include "user.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "vnode.h"
#include "mount.h"
#include "proc.h"
#include "uio.h"
#include "malloc.h"
#include "buf.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"
#include "../netinet/in.h"
#include "../netinet/tcp.h"
#include "nfsv2.h"
#include "nfs.h"
#include "nfsrvcache.h"

/* Global defs. */
extern u_long nfs_prog, nfs_vers;
extern int (*nfsrv_procs[NFS_NPROCS])();
extern struct buf nfs_bqueue;
extern int nfs_asyncdaemons;
extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern int nfs_tcpnodelay;
struct file *getsock();

#define	TRUE	1
#define	FALSE	0

/*
 * NFS server system calls
 * getfh() lives here too, but maybe should move to kern/vfs_syscalls.c
 */

/*
 * Get file handle system call
 */
/* ARGSUSED */
getfh(p, uap, retval)
	struct proc *p;
	register struct args {
		char	*fname;
		fhandle_t *fhp;
	} *uap;
	int *retval;
{
	register struct nameidata *ndp = &u.u_nd;
	register struct vnode *vp;
	fhandle_t fh;
	int error;

	/*
	 * Must be super user
	 */
	if (error = suser(ndp->ni_cred, &u.u_acflag))
		return (error);
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp))
		return (error);
	vp = ndp->ni_vp;
	bzero((caddr_t)&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&fh, (caddr_t)uap->fhp, sizeof (fh));
	return (error);
}

/*
 * Nfs server psuedo system call for the nfsd's
 * Never returns unless it fails or gets killed
 */
/* ARGSUSED */
nfssvc(p, uap, retval)
	struct proc *p;
	register struct args {
		int s;
		caddr_t mskval;
		int msklen;
		caddr_t mtchval;
		int mtchlen;
	} *uap;
	int *retval;
{
	register struct mbuf *m;
	register int siz;
	register struct ucred *cr;
	struct file *fp;
	struct mbuf *mreq, *mrep, *nam, *md;
	struct mbuf msk, mtch;
	struct socket *so;
	caddr_t dpos;
	int procid, repstat, error, cacherep;
	u_long retxid;

	/*
	 * Must be super user
	 */
	if (error = suser(u.u_cred, &u.u_acflag))
		goto bad;
	fp = getsock(uap->s);
	if (fp == 0)
		return;
	so = (struct socket *)fp->f_data;
	if (sosendallatonce(so))
		siz = NFS_MAXPACKET;
	else
		siz = NFS_MAXPACKET + sizeof(u_long);
	if (error = soreserve(so, siz, siz))
		goto bad;
	if (error = sockargs(&nam, uap->mskval, uap->msklen, MT_SONAME))
		goto bad;
	bcopy((caddr_t)nam, (caddr_t)&msk, sizeof (struct mbuf));
	msk.m_data = msk.m_dat;
	m_freem(nam);
	if (error = sockargs(&nam, uap->mtchval, uap->mtchlen, MT_SONAME))
		goto bad;
	bcopy((caddr_t)nam, (caddr_t)&mtch, sizeof (struct mbuf));
	mtch.m_data = mtch.m_dat;
	m_freem(nam);

	/* Copy the cred so others don't see changes */
	cr = u.u_cred = crcopy(u.u_cred);

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		MGET(m, M_WAIT, MT_SOOPTS);
		*mtod(m, int *) = 1;
		m->m_len = sizeof(int);
		sosetopt(so, SOL_SOCKET, SO_KEEPALIVE, m);
	}
	if (so->so_proto->pr_domain->dom_family == AF_INET &&
	    so->so_proto->pr_protocol == IPPROTO_TCP &&
	    nfs_tcpnodelay) {
		MGET(m, M_WAIT, MT_SOOPTS);
		*mtod(m, int *) = 1;
		m->m_len = sizeof(int);
		sosetopt(so, IPPROTO_TCP, TCP_NODELAY, m);
	}
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;

	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
		if (error = nfs_getreq(so, nfs_prog, nfs_vers, NFS_NPROCS-1,
		   &nam, &mrep, &md, &dpos, &retxid, &procid, cr,
		   &msk, &mtch)) {
			if (nam)
				m_freem(nam);
			if (error == EPIPE || error == EINTR ||
			    error == ERESTART) {
				error = 0;
				goto bad;
			}
			so->so_error = 0;
			continue;
		}

		if (nam)
			cacherep = nfsrv_getcache(nam, retxid, procid, &mreq);
		else
			cacherep = RC_DOIT;
		switch (cacherep) {
		case RC_DOIT:
			if (error = (*(nfsrv_procs[procid]))(mrep, md, dpos,
				cr, retxid, &mreq, &repstat)) {
				nfsstats.srv_errs++;
				if (nam) {
					nfsrv_updatecache(nam, retxid, procid,
						FALSE, repstat, mreq);
					m_freem(nam);
				}
				break;
			}
			nfsstats.srvrpccnt[procid]++;
			if (nam)
				nfsrv_updatecache(nam, retxid, procid, TRUE,
					repstat, mreq);
			mrep = (struct mbuf *)0;
		case RC_REPLY:
			m = mreq;
			siz = 0;
			while (m) {
				siz += m->m_len;
				m = m->m_next;
			}
			if (siz <= 0 || siz > NFS_MAXPACKET) {
				printf("mbuf siz=%d\n",siz);
				panic("Bad nfs svc reply");
			}
			mreq->m_pkthdr.len = siz;
			mreq->m_pkthdr.rcvif = (struct ifnet *)0;
			/*
			 * For non-atomic protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (!sosendallatonce(so)) {
				M_PREPEND(mreq, sizeof(u_long), M_WAIT);
				*mtod(mreq, u_long *) = htonl(0x80000000 | siz);
			}
			error = nfs_send(so, nam, mreq, (struct nfsreq *)0);
			if (nam)
				m_freem(nam);
			if (mrep)
				m_freem(mrep);
			if (error) {
				if (error == EPIPE || error == EINTR ||
				    error == ERESTART)
					goto bad;
				so->so_error = 0;
			}
			break;
		case RC_DROPIT:
			m_freem(mrep);
			m_freem(nam);
			break;
		};
	}
bad:
	return (error);
}

/*
 * Nfs pseudo system call for asynchronous i/o daemons.
 * These babies just pretend to be disk interrupt service routines
 * for client nfs. They are mainly here for read ahead/write behind.
 * Never returns unless it fails or gets killed
 */
/* ARGSUSED */
async_daemon(p, uap, retval)
	struct proc *p;
	struct args *uap;
	int *retval;
{
	register struct buf *bp, *dp;
	int error;
	int myiod;

	/*
	 * Must be super user
	 */
	if (error = suser(u.u_cred, &u.u_acflag))
		return (error);
	/*
	 * Assign my position or return error if too many already running
	 */
	if (nfs_asyncdaemons > NFS_MAXASYNCDAEMON)
		return (EBUSY);
	myiod = nfs_asyncdaemons++;
	dp = &nfs_bqueue;
	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
		while (dp->b_actf == NULL) {
			nfs_iodwant[myiod] = p;
			if (error = tsleep((caddr_t)&nfs_iodwant[myiod],
				PWAIT | PCATCH, "nfsidl", 0))
				return (error);
		}
		/* Take one off the end of the list */
		bp = dp->b_actl;
		if (bp->b_actl == dp) {
			dp->b_actf = dp->b_actl = (struct buf *)0;
		} else {
			dp->b_actl = bp->b_actl;
			bp->b_actl->b_actf = dp;
		}
		(void) nfs_doio(bp);
	}
}
