/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)ttinit.c	3.26 (Berkeley) 6/6/90";
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#ifdef POSIX_TTY
#include <sys/ioctl.h>
#endif

int tt_h19();
int tt_h29();
int tt_f100();
int tt_tvi925();
int tt_wyse75();
int tt_wyse60();
int tt_zapple();
int tt_zentec();
int tt_generic();
struct tt_tab tt_tab[] = {
	{ "h19",	3, tt_h19 },
	{ "h29",	3, tt_h29 },
	{ "f100",	4, tt_f100 },
	{ "tvi925",	6, tt_tvi925 },
	{ "wyse75",	6, tt_wyse75 },
	{ "wyse60",	6, tt_wyse60 },
	{ "w60",	3, tt_wyse60 },
	{ "zapple",	6, tt_zapple },
	{ "zentec",	6, tt_zentec },
	{ "generic",	0, tt_generic },
	0
};

ttinit()
{
	int i;
	register struct tt_tab *tp;
	register char *p, *q;
	register char *t;
	struct winsize winsize;
	int ttflush();

	tt_strp = tt_strings;

	/*
	 * Set output buffer size to about 1 second of output time.
	 */
	i = MIN(wwbaud/10, 512);
	if ((tt_ob = malloc((unsigned) i)) == 0) {
		wwerrno = WWE_NOMEM;
		return -1;
	}
	tt_obp = tt_ob;
	tt_obe = tt_ob + i;

	/*
	 * Use the standard name of the terminal (i.e. the second
	 * name in termcap).
	 */
	for (p = wwtermcap; *p && *p != '|' && *p != ':'; p++)
		;
	if (*p == '|')
		p++;
	for (q = p; *q && *q != '|' && *q != ':'; q++)
		;
	if (q != p && (t = malloc((unsigned) (q - p + 1))) != 0) {
		wwterm = t;
		while (p < q)
			*t++ = *p++;
		*t = 0;
	}
	for (tp = tt_tab; tp->tt_name != 0; tp++)
		if (strncmp(tp->tt_name, wwterm, tp->tt_len) == 0)
			break;
	if (tp->tt_name == 0) {
		wwerrno = WWE_BADTERM;
		return -1;
	}
	if ((*tp->tt_func)() < 0) {
		wwerrno = WWE_CANTDO;
		return -1;
	}
	if (ioctl(0, TIOCGWINSZ, (char *)&winsize) >= 0) {
		if (winsize.ws_row != 0)
			tt.tt_nrow = winsize.ws_row;
		if (winsize.ws_col != 0)
			tt.tt_ncol = winsize.ws_col;
	}
	tt.tt_scroll_top = 0;
	tt.tt_scroll_bot = tt.tt_nrow - 1;
	tt.tt_flush = ttflush;
	return 0;
}
