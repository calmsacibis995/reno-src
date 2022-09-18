/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)init_main.c	7.29 (Berkeley) 6/22/90
 */

#include "param.h"
#include "systm.h"
#include "user.h"
#include "kernel.h"
#include "mount.h"
#include "map.h"
#include "proc.h"
#include "vnode.h"
#include "seg.h"
#include "conf.h"
#include "buf.h"
#include "vm.h"
#include "cmap.h"
#include "text.h"
#include "clist.h"
#include "malloc.h"
#include "protosw.h"
#include "reboot.h"
#include "../ufs/quota.h"

#include "machine/pte.h"
#include "machine/reg.h"
#include "machine/cpu.h"

int	cmask = CMASK;
extern	int (*mountroot)();
/*
 * Initialization code.
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 *	     - process 2 to page out
 */
main(firstaddr)
	int firstaddr;
{
	register int i;
	register struct proc *p;
	register struct pgrp *pg;
	int s;

	rqinit();
	startup(firstaddr);

	/*
	 * set up system process 0 (swapper)
	 */
	p = &proc[0];
	bcopy("swapper", p->p_comm, sizeof ("swapper"));
	p->p_p0br = u.u_pcb.pcb_p0br;
	p->p_szpt = 1;
	p->p_addr = uaddr(p);
	p->p_stat = SRUN;
	p->p_flag |= SLOAD|SSYS;
	p->p_nice = NZERO;
	setredzone(p->p_addr, (caddr_t)&u);
	u.u_procp = p;
	MALLOC(pgrphash[0], struct pgrp *, sizeof (struct pgrp), 
		M_PGRP, M_NOWAIT);
	if ((pg = pgrphash[0]) == NULL)
		panic("no space to craft zero'th process group");
	pg->pg_id = 0;
	pg->pg_hforw = 0;
	pg->pg_mem = p;
	pg->pg_jobc = 0;
	p->p_pgrp = pg;
	p->p_pgrpnxt = 0;
	MALLOC(pg->pg_session, struct session *, sizeof (struct session),
		M_SESSION, M_NOWAIT);
	if (pg->pg_session == NULL)
		panic("no space to craft zero'th session");
	pg->pg_session->s_count = 1;
	pg->pg_session->s_leader = NULL;
	pg->pg_session->s_ttyvp = NULL;
	pg->pg_session->s_ttyp = NULL;
#ifdef KTRACE
	p->p_tracep = NULL;
	p->p_traceflag = 0;
#endif
	/*
	 * These assume that the u. area is always mapped 
	 * to the same virtual address. Otherwise must be
	 * handled when copying the u. area in newproc().
	 */
	ndinit(&u.u_nd);

	u.u_cmask = cmask;
	u.u_lastfile = -1;
	for (i = 0; i < sizeof(u.u_rlimit)/sizeof(u.u_rlimit[0]); i++)
		u.u_rlimit[i].rlim_cur = u.u_rlimit[i].rlim_max = 
		    RLIM_INFINITY;
	/*
	 * configure virtual memory system,
	 * set vm rlimits
	 */
	vminit();

	/*
	 * Initialize the file systems.
	 *
	 * Get vnodes for swapdev, argdev, and rootdev.
	 */
	vfsinit();
	if (bdevvp(swapdev, &swapdev_vp) ||
	    bdevvp(argdev, &argdev_vp) ||
	    bdevvp(rootdev, &rootvp))
		panic("can't setup bdevvp's");

	/*
	 * Setup credentials
	 */
	u.u_cred = crget();
	u.u_cred->cr_ngroups = 1;

	startrtclock();
#if defined(vax)
#include "kg.h"
#if NKG > 0
	startkgclock();
#endif
#endif

	/*
	 * Initialize tables, protocols, and set up well-known inodes.
	 */
	mbinit();
	cinit();
#ifdef SYSVSHM
	shminit();
#endif
#include "sl.h"
#if NSL > 0
	slattach();			/* XXX */
#endif
#include "loop.h"
#if NLOOP > 0
	loattach();			/* XXX */
#endif
	/*
	 * Block reception of incoming packets
	 * until protocols have been initialized.
	 */
	s = splimp();
	ifinit();
	domaininit();
	splx(s);
	pqinit();
	xinit();
	swapinit();
#ifdef GPROF
	kmstartup();
#endif

/* kick off timeout driven events by calling first time */
	roundrobin();
	schedcpu();
	schedpaging();

/* set up the root file system */
	if ((*mountroot)())
		panic("cannot mount root");
	/*
	 * Get vnode for '/'.
	 * Setup rootdir and u.u_cdir to point to it.
	 */
	if (VFS_ROOT(rootfs, &rootdir))
		panic("cannot find root vnode");
	u.u_cdir = rootdir;
	VREF(u.u_cdir);
	VOP_UNLOCK(rootdir);
	u.u_rdir = NULL;
	boottime = u.u_start =  time;
	u.u_dmap = zdmap;
	u.u_smap = zdmap;

	enablertclock();		/* enable realtime clock interrupts */
	/*
	 * make init process
	 */

	siginit(&proc[0]);
	proc[0].p_szpt = CLSIZE;
	if (newproc(0)) {
		expand(clrnd((int)btoc(szicode)), 0);
		(void) swpexpand(u.u_dsize, (segsz_t)0, &u.u_dmap, &u.u_smap);
		(void) copyout((caddr_t)icode, (caddr_t)0, (unsigned)szicode);
		/*
		 * Return goes to loc. 0 of user init
		 * code just copied out.
		 */
		return;
	}
	/*
	 * make page-out daemon (process 2)
	 * the daemon has ctopt(nswbuf*CLSIZE*KLMAX) pages of page
	 * table so that it can map dirty pages into
	 * its address space during asychronous pushes.
	 */
	proc[0].p_szpt = clrnd(ctopt(nswbuf*CLSIZE*KLMAX + HIGHPAGES));
	if (newproc(0)) {
		proc[2].p_flag |= SLOAD|SSYS;
		proc[2].p_dsize = u.u_dsize = nswbuf*CLSIZE*KLMAX; 
		bcopy("pagedaemon", proc[2].p_comm, sizeof ("pagedaemon"));
		pageout();
		/*NOTREACHED*/
	}

	/*
	 * enter scheduling loop
	 */
	proc[0].p_szpt = 1;
	sched();
}

/*
 * Initialize hash links for buffers.
 */
bhinit()
{
	register int i;
	register struct bufhd *bp;

	for (bp = bufhash, i = 0; i < BUFHSZ; i++, bp++)
		bp->b_forw = bp->b_back = (struct buf *)bp;
}

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */
binit()
{
	register struct buf *bp, *dp;
	register int i;
	int base, residual;

	for (dp = bfreelist; dp < &bfreelist[BQUEUES]; dp++) {
		dp->b_forw = dp->b_back = dp->av_forw = dp->av_back = dp;
		dp->b_flags = B_HEAD;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bp->b_dev = NODEV;
		bp->b_bcount = 0;
		bp->b_rcred = NOCRED;
		bp->b_wcred = NOCRED;
		bp->b_dirtyoff = 0;
		bp->b_dirtyend = 0;
		bp->b_un.b_addr = buffers + i * MAXBSIZE;
		if (i < residual)
			bp->b_bufsize = (base + 1) * CLBYTES;
		else
			bp->b_bufsize = base * CLBYTES;
		binshash(bp, &bfreelist[BQ_AGE]);
		bp->b_flags = B_BUSY|B_INVAL;
		brelse(bp);
	}
}

/*
 * Set up swap devices.
 * Initialize linked list of free swap
 * headers. These do not actually point
 * to buffers, but rather to pages that
 * are being swapped in and out.
 */
swapinit()
{
	register int i;
	register struct buf *sp = swbuf;
	struct swdevt *swp;
	int error;

	/*
	 * Count swap devices, and adjust total swap space available.
	 * Some of this space will not be available until a swapon()
	 * system is issued, usually when the system goes multi-user.
	 */
	nswdev = 0;
	nswap = 0;
	for (swp = swdevt; swp->sw_dev; swp++) {
		nswdev++;
		if (swp->sw_nblks > nswap)
			nswap = swp->sw_nblks;
	}
	if (nswdev == 0)
		panic("swapinit");
	if (nswdev > 1)
		nswap = ((nswap + dmmax - 1) / dmmax) * dmmax;
	nswap *= nswdev;
	/*
	 * If there are multiple swap areas,
	 * allow more paging operations per second.
	 */
	if (nswdev > 1)
		maxpgio = (maxpgio * (2 * nswdev - 1)) / 2;
	if (bdevvp(swdevt[0].sw_dev, &swdevt[0].sw_vp))
		panic("swapvp");
	if (error = swfree(0)) {
		printf("swfree errno %d\n", error);	/* XXX */
		panic("swapinit swfree 0");
	}

	/*
	 * Now set up swap buffer headers.
	 */
	bswlist.av_forw = sp;
	for (i=0; i<nswbuf-1; i++, sp++)
		sp->av_forw = sp+1;
	sp->av_forw = NULL;
}

/*
 * Initialize clist by freeing all character blocks, then count
 * number of character devices. (Once-only routine)
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;

	ccp = (int)cfree;
	ccp = (ccp+CROUND) & ~CROUND;
	for(cp=(struct cblock *)ccp; cp < &cfree[nclist-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
		cfreecount += CBSIZE;
	}
}
