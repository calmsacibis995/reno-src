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
 *	@(#)fhpib.c	7.1 (Berkeley) 5/8/90
 */

/*
 * 98625A/B HPIB driver
 */

#include "param.h"
#include "../hpdev/fhpibreg.h"
#include "hpibvar.h"

fhpibinit(unit)
	register int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd = (struct fhpibdevice *)hs->sc_addr;

	if (hd->hpib_cid != HPIBC)
		return(0);
	hs->sc_type = HPIBC;
	hs->sc_ba = HPIBC_BA;
	fhpibreset(unit);
	return(1);
}

fhpibreset(unit)
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;

	hd = (struct fhpibdevice *)hs->sc_addr;
	hd->hpib_cid = 0xFF;
	DELAY(100);
	hd->hpib_cmd = CT_8BIT;
	hd->hpib_ar = AR_ARONC;
	hd->hpib_cmd |= CT_IFC;
	hd->hpib_cmd |= CT_INITFIFO;
	DELAY(100);
	hd->hpib_cmd &= ~CT_IFC;
	hd->hpib_cmd |= CT_REN;
	hd->hpib_stat = ST_ATN;
	hd->hpib_data = C_DCL;
	DELAY(100000);
}

fhpibsend(unit, slave, sec, buf, cnt)
	register char *buf;
	register int cnt;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	int origcnt = cnt;

	hd = (struct fhpibdevice *)hs->sc_addr;
	hd->hpib_stat = 0;
	hd->hpib_imask = IM_IDLE | IM_ROOM;
	fhpibwait(hd, IM_IDLE);
	hd->hpib_stat = ST_ATN;
	hd->hpib_data = C_UNL;
	hd->hpib_data = C_TAG + hs->sc_ba;
	hd->hpib_data = C_LAG + slave;
	if (sec != -1)
		hd->hpib_data = C_SCG + sec;
	fhpibwait(hd, IM_IDLE);
	hd->hpib_stat = ST_WRITE;
	if (cnt) {
		while (--cnt) {
			hd->hpib_data = *buf++;
			if (fhpibwait(hd, IM_ROOM) < 0)
				break;
		}
		hd->hpib_stat = ST_EOI;
		hd->hpib_data = *buf;
		if (fhpibwait(hd, IM_ROOM) < 0)
			cnt++;
		hd->hpib_stat = ST_ATN;
		/* XXX: HP-UX claims bug with CS80 transparent messages */
		if (sec == 0x12)
			DELAY(150);
		hd->hpib_data = C_UNL;
		fhpibwait(hd, IM_IDLE);
	}
	hd->hpib_imask = 0;
	return(origcnt - cnt);
}

fhpibrecv(unit, slave, sec, buf, cnt)
	register char *buf;
	register int cnt;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	int origcnt = cnt;

	hd = (struct fhpibdevice *)hs->sc_addr;
	hd->hpib_stat = 0;
	hd->hpib_imask = IM_IDLE | IM_ROOM | IM_BYTE;
	fhpibwait(hd, IM_IDLE);
	hd->hpib_stat = ST_ATN;
	hd->hpib_data = C_UNL;
	hd->hpib_data = C_LAG + hs->sc_ba;
	hd->hpib_data = C_TAG + slave;
	if (sec != -1)
		hd->hpib_data = C_SCG + sec;
	fhpibwait(hd, IM_IDLE);
	hd->hpib_stat = ST_READ0;
	hd->hpib_data = 0;
	if (cnt) {
		while (--cnt >= 0) {
			if (fhpibwait(hd, IM_BYTE) < 0)
				break;
			*buf++ = hd->hpib_data;
		}
		cnt++;
		fhpibwait(hd, IM_ROOM);
		hd->hpib_stat = ST_ATN;
		hd->hpib_data = (slave == 31) ? C_UNA : C_UNT;
		fhpibwait(hd, IM_IDLE);
	}
	hd->hpib_imask = 0;
	return(origcnt - cnt);
}

fhpibppoll(unit)
	register int unit;
{
	register struct hpib_softc *hs = &hpib_softc[unit];
	register struct fhpibdevice *hd;
	register int ppoll;

	hd = (struct fhpibdevice *)hs->sc_addr;
	hd->hpib_stat = 0;
	hd->hpib_psense = 0;
	hd->hpib_pmask = 0xFF;
	hd->hpib_imask = IM_PPRESP | IM_PABORT;
	DELAY(25);
	hd->hpib_intr = IM_PABORT;
	ppoll = hd->hpib_data;
	if (hd->hpib_intr & IM_PABORT)
		ppoll = 0;
	hd->hpib_imask = 0;
	hd->hpib_pmask = 0;
	hd->hpib_stat = ST_IENAB;
	return(ppoll);
}

fhpibwait(hd, x)
	register struct fhpibdevice *hd;
{
	register int timo = 100000;

	while ((hd->hpib_intr & x) == 0 && --timo)
		;
	if (timo == 0)
		return(-1);
	return(0);
}
