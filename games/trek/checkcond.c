/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)checkcond.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"trek.h"

/*
**  Check for Condition After a Move
**
**	Various ship conditions are checked.  First we check
**	to see if we have already lost the game, due to running
**	out of life support reserves, running out of energy,
**	or running out of crew members.  The check for running
**	out of time is in events().
**
**	If we are in automatic override mode (Etc.nkling < 0), we
**	don't want to do anything else, lest we call autover
**	recursively.
**
**	In the normal case, if there is a supernova, we call
**	autover() to help us escape.  If after calling autover()
**	we are still in the grips of a supernova, we get burnt
**	up.
**
**	If there are no Klingons in this quadrant, we nullify any
**	distress calls which might exist.
**
**	We then set the condition code, based on the energy level
**	and battle conditions.
*/

checkcond()
{
	register int		i, j;

	/* see if we are still alive and well */
	if (Ship.reserves < 0.0)
		lose(L_NOLIFE);
	if (Ship.energy <= 0)
		lose(L_NOENGY);
	if (Ship.crew <= 0)
		lose(L_NOCREW);
	/* if in auto override mode, ignore the rest */
	if (Etc.nkling < 0)
		return;
	/* call in automatic override if appropriate */
	if (Quad[Ship.quadx][Ship.quady].stars < 0)
		autover();
	if (Quad[Ship.quadx][Ship.quady].stars < 0)
		lose(L_SNOVA);
	/* nullify distress call if appropriate */
	if (Etc.nkling <= 0)
		killd(Ship.quadx, Ship.quady, 1);

	/* set condition code */
	if (Ship.cond == DOCKED)
		return;

	if (Etc.nkling > 0)
	{
		Ship.cond = RED;
		return;
	}
	if (Ship.energy < Param.energylow)
	{
		Ship.cond = YELLOW;
		return;
	}
	Ship.cond = GREEN;
	return;
}
