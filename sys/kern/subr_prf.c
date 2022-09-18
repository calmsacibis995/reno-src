/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)subr_prf.c	7.18 (Berkeley) 6/28/90
 */

#include "param.h"
#include "systm.h"
#include "seg.h"
#include "buf.h"
#include "conf.h"
#include "reboot.h"
#include "vm.h"
#include "msgbuf.h"
#include "user.h"
#include "proc.h"
#include "ioctl.h"
#include "vnode.h"
#include "file.h"
#include "tty.h"
#include "tprintf.h"
#include "syslog.h"
#include "malloc.h"

#include "machine/mtpr.h"
#ifdef KADB
#include "machine/kdbparam.h"
#endif

#define TOCONS	0x1
#define TOTTY	0x2
#define TOLOG	0x4

/*
 * In case console is off,
 * panicstr contains argument to last
 * call to panic.
 */
char	*panicstr;

extern	cnputc();			/* standard console putc */
int	(*v_putc)() = cnputc;		/* routine to putc on virtual console */
extern	struct tty cons;		/* standard console tty */
struct	tty *constty;			/* pointer to console "window" tty */

#ifdef KADB
extern	cngetc();			/* standard console getc */
extern	cnpoll();
int	(*v_getc)() = cngetc;		/* "" getc from virtual console */
int	(*v_poll)() = cnpoll;		/* kdb hook to enable input polling */
#endif

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.  Printf should not be used for chit-chat.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 *
 * Another additional format: %r is used to pass an additional format string
 * and argument list recursively.  Usage is typically:
 *
 * fn(otherstuff, fmt [, arg1, ... ] )
 *	char *fmt;
 *	u_int arg1, ...;
 *
 *	printf("prefix: %r, other stuff\n", fmt, &arg1);
 */
#if defined(tahoe)
int	consintr;
#endif

/*VARARGS1*/
printf(fmt, x1)
	char *fmt;
	unsigned x1;
{
#if defined(tahoe)
	register int savintr;

	savintr = consintr, consintr = 0;	/* disable interrupts */
#endif
	prf(fmt, &x1, TOCONS | TOLOG, (struct tty *)NULL);
	if (!panicstr)
		logwakeup();
#if defined(tahoe)
	consintr = savintr;			/* reenable interrupts */
#endif
}

/*
 * Uprintf prints to the controlling terminal for the current process.
 * It may block if the tty queue is overfull.
 * No message is printed if the queue does not clear
 * in a reasonable time.
 */
/*VARARGS1*/
uprintf(fmt, x1)
	char *fmt;
	unsigned x1;
{
	register struct proc *p = u.u_procp;

	if (p->p_flag & SCTTY && p->p_session->s_ttyvp)
		prf(fmt, &x1, TOTTY, p->p_session->s_ttyp);
}

tpr_t
tprintf_open()
{
	register struct proc *p = u.u_procp;

	if (p->p_flag & SCTTY && p->p_session->s_ttyvp) {
		SESSHOLD(p->p_session);
		return ((tpr_t)p->p_session);
	} else
		return ((tpr_t)NULL);
}

tprintf_close(sess)
	tpr_t sess;
{
	if (sess)
		SESSRELE(sess);
}

/*
 * tprintf prints on the controlling terminal associated
 * with the given session.  
 */
/*VARARGS2*/
tprintf(sess, fmt, x1)
	register tpr_t sess;
	char *fmt;
	unsigned x1;
{
	int flags = TOLOG;

	logpri(LOG_INFO);

	if (sess && sess->s_ttyvp && ttycheckoutq(sess->s_ttyp, 0))
		flags |= TOTTY;
	prf(fmt, &x1, flags, sess->s_ttyp);
	logwakeup();
}


/*
 * Log writes to the log buffer,
 * and guarantees not to sleep (so can be called by interrupt routines).
 * If there is no process reading the log yet, it writes to the console also.
 */
/*VARARGS2*/
log(level, fmt, x1)
	char *fmt;
	unsigned x1;
{
	register s = splhigh();
	extern int log_open;

	logpri(level);
	prf(fmt, &x1, TOLOG, (struct tty *)NULL);
	splx(s);
	if (!log_open)
		prf(fmt, &x1, TOCONS, (struct tty *)NULL);
	logwakeup();
}

logpri(level)
	int level;
{

	putchar('<', TOLOG, (struct tty *)NULL);
	printn((u_long)level, 10, TOLOG, (struct tty *)NULL);
	putchar('>', TOLOG, (struct tty *)NULL);
}

/*VARARGS1*/
addlog(fmt, x1)
	char *fmt;
	unsigned x1;
{
	register s = splhigh();

	prf(fmt, &x1, TOLOG, (struct tty *)NULL);
	splx(s);
	if (!log_open)
		prf(fmt, &x1, TOCONS, (struct tty *)NULL);
	logwakeup();
}

prf(fmt, adx, flags, ttyp)
	register char *fmt;
	register u_int *adx;
	struct tty *ttyp;
{
	register int b, c, i;
	char *s;
	int any;

loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
		putchar(c, flags, ttyp);
	}
again:
	c = *fmt++;
	/* THIS CODE IS MACHINE DEPENDENT IN HANDLING %l? AND %c */
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
		b = -10;
		goto number;
	case 'u':
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn((u_long)*adx, b, flags, ttyp);
		break;
	case 'c':
		b = *adx;
#if BYTE_ORDER == LITTLE_ENDIAN
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0x7f)
				putchar(c, flags, ttyp);
#endif
#if BYTE_ORDER == BIG_ENDIAN
		if (c = (b & 0x7f))
			putchar(c, flags, ttyp);
#endif
		break;
	case 'b':
		b = *adx++;
		s = (char *)*adx;
		printn((u_long)b, *s++, flags, ttyp);
		any = 0;
		if (b) {
			while (i = *s++) {
				if (b & (1 << (i-1))) {
					putchar(any ? ',' : '<', flags, ttyp);
					any = 1;
					for (; (c = *s) > 32; s++)
						putchar(c, flags, ttyp);
				} else
					for (; *s > 32; s++)
						;
			}
			if (any)
				putchar('>', flags, ttyp);
		}
		break;

	case 's':
		s = (char *)*adx;
		while (c = *s++)
			putchar(c, flags, ttyp);
		break;

	case 'r':
		s = (char *)*adx++;
		prf(s, (u_int *)*adx, flags, ttyp);
		break;

	case '%':
		putchar('%', flags, ttyp);
		break;
	}
	adx++;
	goto loop;
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
printn(n, b, flags, ttyp)
	u_long n;
	struct tty *ttyp;
{
	char prbuf[11];
	register char *cp;

	if (b == -10) {
		if ((int)n < 0) {
			putchar('-', flags, ttyp);
			n = (unsigned)(-(int)n);
		}
		b = -b;
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		putchar(*--cp, flags, ttyp);
	while (cp > prbuf);
}

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then reboots.
 * If we are called twice, then we avoid trying to
 * sync the disks as this often leads to recursive panics.
 */
panic(s)
	char *s;
{
	int bootopt = RB_AUTOBOOT | RB_DUMP;

	if (panicstr)
		bootopt |= RB_NOSYNC;
	else {
		panicstr = s;
	}
	printf("panic: %s\n", s);
#ifdef KADB
	if (boothowto & RB_KDB) {
		int x = splnet();	/* below kdb pri */

		setsoftkdb();
		splx(x);
	}
#endif
	boot(bootopt);
}

/*
 * Warn that a system table is full.
 */
tablefull(tab)
	char *tab;
{

	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * Print a character on console or users terminal.
 * If destination is console then the last MSGBUFS characters
 * are saved in msgbuf for inspection later.
 */
/*ARGSUSED*/
putchar(c, flags, ttyp)
	register int c;
	struct tty *ttyp;
{
	extern int msgbufmapped;

	if (panicstr)
		constty = 0;
	if ((flags & TOCONS) && ttyp == NULL && constty) {
		ttyp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOTTY) && ttyp && tputchar(c, ttyp) < 0 &&
	    (flags & TOCONS) && ttyp == constty)
		constty = 0;
	if ((flags & TOLOG) && c != '\0' && c != '\r' && c != 0177 &&
	    msgbufmapped) {
		if (msgbuf.msg_magic != MSG_MAGIC) {
			register int i;

			msgbuf.msg_magic = MSG_MAGIC;
			msgbuf.msg_bufx = msgbuf.msg_bufr = 0;
			for (i=0; i < MSG_BSIZE; i++)
				msgbuf.msg_bufc[i] = 0;
		}
		msgbuf.msg_bufc[msgbuf.msg_bufx++] = c;
		if (msgbuf.msg_bufx < 0 || msgbuf.msg_bufx >= MSG_BSIZE)
			msgbuf.msg_bufx = 0;
	}
	if ((flags & TOCONS) && constty == 0 && c != '\0')
		(*v_putc)(c);
}
