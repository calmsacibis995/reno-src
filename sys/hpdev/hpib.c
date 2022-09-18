/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)hpib.c	7.1 (Berkeley) 5/8/90
 */

/*
 * HPIB driver
 */
#include "hpib.h"
#if NHPIB > 0

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "device.h"
#include "hpibvar.h"
#include "dmavar.h"

#include "machine/cpu.h"
#include "machine/isr.h"

int	internalhpib = IOV(0x478000);

int	hpibinit(), hpibstart(), hpibgo(), hpibintr(), hpibdone();
struct	driver hpibdriver = {
	hpibinit, "hpib", hpibstart, hpibgo, hpibintr, hpibdone,
};

struct	hpib_softc hpib_softc[NHPIB];
struct	isr hpib_isr[NHPIB];
int	nhpibppoll(), fhpibppoll();

int	hpibtimeout = 100000;	/* # of status tests before we give up */
int	hpibidtimeout = 20000;	/* # of status tests for hpibid() calls */

hpibinit(hc)
	register struct hp_ctlr *hc;
{
	register struct hpib_softc *hs = &hpib_softc[hc->hp_unit];
	
	if (!nhpibtype(hc) && !fhpibtype(hc))
		return(0);
	hs->sc_hc = hc;
	hs->sc_dq.dq_unit = hc->hp_unit;
	hs->sc_dq.dq_driver = &hpibdriver;
	hs->sc_sq.dq_forw = hs->sc_sq.dq_back = &hs->sc_sq;
	hpib_isr[hc->hp_unit].isr_intr = hpibintr;
	hpib_isr[hc->hp_unit].isr_ipl = hc->hp_ipl;
	hpib_isr[hc->hp_unit].isr_arg = hc->hp_unit;
	isrlink(&hpib_isr[hc->hp_unit]);
	hpibreset(hc->hp_unit);
	return(1);
}

hpibreset(unit)
	register int unit;
{
	if (hpib_softc[unit].sc_type == HPIBC)
		fhpibreset(unit);
	else
		nhpibreset(unit);
}

hpibreq(dq)
	register struct devqueue *dq;
{
	register struct devqueue *hq;

	hq = &hpib_softc[dq->dq_ctlr].sc_sq;
	insque(dq, hq->dq_back);
	if (dq->dq_back == hq)
		return(1);
	return(0);
}

hpibfree(dq)
	register struct devqueue *dq;
{
	register struct devqueue *hq;

	hq = &hpib_softc[dq->dq_ctlr].sc_sq;
	remque(dq);
	if ((dq = hq->dq_forw) != hq)
		(dq->dq_driver->d_start)(dq->dq_unit);
}

hpibid(unit, slave)
{
	short id;
	int ohpibtimeout;

	/*
	 * XXX: shorten timeout value (so autoconfig doesn't take forever)
	 */
	ohpibtimeout = hpibtimeout;
	hpibtimeout = hpibidtimeout;
	if (hpibrecv(unit, 31, slave, &id, 2) != 2)
		id = 0;
	hpibtimeout = ohpibtimeout;
	return(id);
}

hpibsend(unit, slave, sec, addr, cnt)
	register int unit;
{
	if (hpib_softc[unit].sc_type == HPIBC)
		return(fhpibsend(unit, slave, sec, addr, cnt));
	else
		return(nhpibsend(unit, slave, sec, addr, cnt));
}

hpibrecv(unit, slave, sec, addr, cnt)
	register int unit;
{
	if (hpib_softc[unit].sc_type == HPIBC)
		return(fhpibrecv(unit, slave, sec, addr, cnt));
	else
		return(nhpibrecv(unit, slave, sec, addr, cnt));
}

hpibpptest(unit, slave)
	register int unit;
{
	int (*ppoll)();

	ppoll = (hpib_softc[unit].sc_type == HPIBC) ? fhpibppoll : nhpibppoll;
	return((*ppoll)(unit) & (0x80 >> slave));
}

hpibawait(unit)
{
	register struct hpib_softc *hs = &hpib_softc[unit];

	hs->sc_flags |= HPIBF_PPOLL;
	if (hs->sc_type == HPIBC)
		fhpibppwatch(unit);
	else
		nhpibppwatch(unit);
}

hpibswait(unit, slave)
	register int unit;
{
	register int timo = hpibtimeout;
	register int mask, (*ppoll)();

	ppoll = (hpib_softc[unit].sc_type == HPIBC) ? fhpibppoll : nhpibppoll;
	mask = 0x80 >> slave;
	while (((ppoll)(unit) & mask) == 0)
		if (--timo == 0) {
			printf("hpib%d: swait timeout\n", unit);
			return(-1);
		}
	return(0);
}

hpibustart(unit)
{
	register struct hpib_softc *hs = &hpib_softc[unit];

	if (hs->sc_type == HPIBA)
		hs->sc_dq.dq_ctlr = DMA0;
	else
		hs->sc_dq.dq_ctlr = DMA0 | DMA1;
	if (dmareq(&hs->sc_dq))
		return(1);
	return(0);
}

hpibstart(unit)
{
	register struct devqueue *dq;
	
	dq = hpib_softc[unit].sc_sq.dq_forw;
	(dq->dq_driver->d_go)(dq->dq_unit);
}

hpibgo(unit, slave, sec, addr, count, rw)
	register int unit;
{
	if (hpib_softc[unit].sc_type == HPIBC)
		fhpibgo(unit, slave, sec, addr, count, rw);
	else
		nhpibgo(unit, slave, sec, addr, count, rw);
}

hpibdone(unit)
	register int unit;
{
	if (hpib_softc[unit].sc_type == HPIBC)
		fhpibdone(unit);
	else
		nhpibdone(unit);
}

hpibintr(unit)
	register int unit;
{
	int found;

	if (hpib_softc[unit].sc_type == HPIBC)
		found = fhpibintr(unit);
	else
		found = nhpibintr(unit);
	return(found);
}
#endif
