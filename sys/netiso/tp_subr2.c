/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */
/* 
 * ARGO TP
 *
 * $Header: tp_subr2.c,v 5.5 88/11/18 17:28:55 nhall Exp $
 * $Source: /usr/argo/sys/netiso/RCS/tp_subr2.c,v $
 *	@(#)tp_subr2.c	7.4 (Berkeley) 9/22/89
 *
 * Some auxiliary routines:
 * 		tp_protocol_error: required by xebec- called when a combo of state,
 *			event, predicate isn't covered for by the transition file.
 *		tp_indicate: gives indications(signals) to the user process
 *		tp_getoptions: initializes variables that are affected by the options
 *          chosen.
 */

#ifndef lint
static char *rcsid = "$Header: tp_subr2.c,v 5.5 88/11/18 17:28:55 nhall Exp $";
#endif lint

#include "argoxtwentyfive.h"

/* this def'n is to cause the expansion of this macro in the
 * routine tp_local_credit :
 */
#define LOCAL_CREDIT_EXPAND

#include "param.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"
#include "errno.h"
#include "types.h"
#include "time.h"
#include "kernel.h"
#undef MNULL
#include "argo_debug.h"
#include "tp_param.h"
#include "tp_ip.h"
#include "iso.h"
#include "iso_errno.h"
#include "iso_pcb.h"
#include "tp_timer.h"
#include "tp_stat.h"
#include "tp_tpdu.h"
#include "tp_pcb.h"
#include "tp_seq.h"
#include "tp_trace.h"
#include "tp_user.h"
#include "cons.h"

/*
 * NAME: 	tp_local_credit()
 *
 * CALLED FROM:
 *  tp_emit(), tp_usrreq()
 *
 * FUNCTION and ARGUMENTS:
 *	Computes the local credit and stashes it in tpcb->tp_lcredit.
 *  It's a macro in the production system rather than a procdure.
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This doesn't actually get called in a production system - 
 *  the macro gets expanded instead in place of calls to this proc.
 *  But for debugging, we call this and that allows us to add
 *  debugging messages easily here.
 */
void
tp_local_credit(tpcb)
	struct tp_pcb *tpcb;
{
	LOCAL_CREDIT(tpcb);
	IFDEBUG(D_CREDIT)
		printf("ref 0x%x lcdt 0x%x l_tpdusize 0x%x decbit 0x%x\n",
			tpcb->tp_refp - tp_ref, 
			tpcb->tp_lcredit, 
			tpcb->tp_l_tpdusize, 
			tpcb->tp_decbit, 
			tpcb->tp_cong_win
			);
	ENDDEBUG
	IFTRACE(D_CREDIT)
		tptraceTPCB(TPPTmisc,
			"lcdt tpdusz \n",
			 tpcb->tp_lcredit, tpcb->tp_l_tpdusize, 0, 0);
	ENDTRACE
}

/*
 * NAME:  tp_protocol_error()
 *
 * CALLED FROM:
 *  tp_driver(), when it doesn't know what to do with
 * 	a combo of event, state, predicate
 *
 * FUNCTION and ARGUMENTS:
 *  print error mesg 
 *
 * RETURN VALUE:
 *  EIO - always
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
tp_protocol_error(e,tpcb)
	struct tp_event	*e;
	struct tp_pcb	*tpcb;
{
	printf("TP PROTOCOL ERROR! tpcb 0x%x event 0x%x, state 0x%x\n",
		tpcb, e->ev_number, tpcb->tp_state);
	IFTRACE(D_DRIVER)
		tptraceTPCB(TPPTmisc, "PROTOCOL ERROR tpcb event state",
			tpcb, e->ev_number, tpcb->tp_state, 0 );
	ENDTRACE
	return EIO; /* for lack of anything better */
}


/* Not used at the moment */
ProtoHook
tp_drain()
{
	return 0;
}


/*
 * NAME: tp_indicate()
 *
 * CALLED FROM:
 * 	tp.trans when XPD arrive, when a connection is being disconnected by
 *  the arrival of a DR or ER, and when a connection times out.
 *
 * FUNCTION and ARGUMENTS:
 *  (ind) is the type of indication : T_DISCONNECT, T_XPD
 *  (error) is an E* value that will be put in the socket structure
 *  to be passed along to the user later.
 * 	Gives a SIGURG to the user process or group indicated by the socket
 * 	attached to the tpcb.
 *
 * RETURNS:  Rien
 * 
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_indicate(ind, tpcb, error)
	int				ind; 
	u_short			error;
	register struct tp_pcb	*tpcb;
{
	register struct socket *so = tpcb->tp_sock;
	IFTRACE(D_INDICATION)
		tptraceTPCB(TPPTindicate, ind, *(u_short *)(tpcb->tp_lsuffix), 
			*(u_short *)(tpcb->tp_fsuffix), error,so->so_pgid);
	ENDTRACE
	IFDEBUG(D_INDICATION)
		char *ls, *fs;
		ls = tpcb->tp_lsuffix, 
		fs = tpcb->tp_fsuffix, 

		printf(
"indicate 0x%x lsuf 0x%02x%02x fsuf 0x%02x%02x err 0x%x  noind 0x%x ref 0x%x\n",
		ind, 
		*ls, *(ls+1), *fs, *(fs+1),
		error, /*so->so_pgrp,*/
		tpcb->tp_no_disc_indications,
		tpcb->tp_lref);
	ENDDEBUG

	so->so_error = error;

	if (ind == T_DISCONNECT)  {
		if ( tpcb->tp_no_disc_indications )
			return;
	}
	IFTRACE(D_INDICATION)
		tptraceTPCB(TPPTmisc, "doing sohasoutofband(so)", so,0,0,0);
	ENDTRACE
	sohasoutofband(so);
}

/*
 * NAME : tp_getoptions()
 *
 * CALLED FROM:
 * 	tp.trans whenever we go into OPEN state 
 *
 * FUNCTION and ARGUMENTS:
 *  sets the proper flags and values in the tpcb, to control
 *  the appropriate actions for the given class, options,
 *  sequence space, etc, etc.
 * 
 * RETURNS: Nada
 * 
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_getoptions(tpcb)
struct tp_pcb *tpcb;
{
	tpcb->tp_seqmask = 
		tpcb->tp_xtd_format ?	TP_XTD_FMT_MASK :	TP_NML_FMT_MASK ;
	tpcb->tp_seqbit =
		tpcb->tp_xtd_format ?	TP_XTD_FMT_BIT :	TP_NML_FMT_BIT ;
	tpcb->tp_seqhalf = tpcb->tp_seqbit >> 1;
	tpcb->tp_dt_ticks =
		MAX(tpcb->tp_dt_ticks, (tpcb->tp_peer_acktime + 2));

}

/*
 * NAME:  tp_recycle_tsuffix()
 *
 * CALLED FROM:
 *  Called when a ref is frozen.
 *
 * FUNCTION and ARGUMENTS:
 *  allows the suffix to be reused. 
 *
 * RETURNS: zilch
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_recycle_tsuffix(tpcb)
	struct tp_pcb	*tpcb;
{
	bzero((caddr_t)tpcb->tp_lsuffix, sizeof( tpcb->tp_lsuffix));
	bzero((caddr_t)tpcb->tp_fsuffix, sizeof( tpcb->tp_fsuffix));
	tpcb->tp_fsuffixlen = tpcb->tp_lsuffixlen = 0;

	(tpcb->tp_nlproto->nlp_recycle_suffix)(tpcb->tp_npcb);
}

/*
 * NAME: tp_quench()
 *
 * CALLED FROM:
 *  tp{af}_quench() when ICMP source quench or similar thing arrives.
 *
 * FUNCTION and ARGUMENTS:
 *  Drop the congestion window back to 1.
 *  Congestion window scheme:
 *  Initial value is 1.  ("slow start" as Nagle, et. al. call it)
 *  For each good ack that arrives, the congestion window is increased
 *  by 1 (up to max size of logical infinity, which is to say, 
 *	it doesn't wrap around).
 *  Source quench causes it to drop back to 1.
 *  tp_send() uses the smaller of (regular window, congestion window). 
 *  One retransmission strategy option is to have any retransmission 
 *	cause reset the congestion window back  to 1.
 *
 *	(cmd) is either PRC_QUENCH: source quench, or
 *		PRC_QUENCH2: dest. quench (dec bit)
 *
 * RETURNS:
 * 
 * SIDE EFFECTS:
 * 
 * NOTES:
 */
void
tp_quench( tpcb, cmd )
	struct tp_pcb *tpcb;
	int cmd;
{
	IFDEBUG(D_QUENCH)
		printf("tp_quench tpcb 0x%x ref 0x%x sufx 0x%x\n",
			tpcb, tpcb->tp_lref, *(u_short *)(tpcb->tp_lsuffix));
		printf("cong_win 0x%x decbit 0x%x \n",
			tpcb->tp_cong_win, tpcb->tp_decbit);
	ENDDEBUG
	switch(cmd) {
		case PRC_QUENCH:
			tpcb->tp_cong_win = 1;
			IncStat(ts_quench);
			break;
		case PRC_QUENCH2:
			tpcb->tp_cong_win = 1; /* might as well quench source also */
			tpcb->tp_decbit = TP_DECBIT_CLEAR_COUNT;
			IncStat(ts_rcvdecbit);
			break;
	}
}


/*
 * NAME:	tp_netcmd()
 *
 * CALLED FROM:			
 *
 * FUNCTION and ARGUMENTS:			
 *
 * RETURNS:			
 *
 * SIDE EFFECTS:	
 *
 * NOTES:			
 */
tp_netcmd( tpcb, cmd )
	struct tp_pcb *tpcb;
	int cmd;
{
#if NARGOXTWENTYFIVE > 0
	switch (cmd) {

	case CONN_CLOSE:
	case CONN_REFUSE:
		cons_netcmd( cmd, tpcb->tp_npcb, 0, tpcb->tp_class == TP_CLASS_4);
		/* TODO: can this last param be replaced by 
	 	*	tpcb->tp_netserv != ISO_CONS?)
		*/
		break;

	default:
		printf("tp_netcmd(0x%x, 0x%x) NOT IMPLEMENTED\n", tpcb, cmd);
		break;
	}
#else NARGOXTWENTYFIVE
	printf("tp_netcmd(): X25 NOT CONFIGURED!!\n");
#endif NARGOXTWENTYFIVE > 0
}
/*
 * CALLED FROM:
 *  tp_ctloutput() and tp_emit()
 * FUNCTION and ARGUMENTS:
 * 	Convert a class mask to the highest numeric value it represents.
 */

int
tp_mask_to_num(x)
	u_char x;
{
	register int j;

	for(j = 4; j>=0 ;j--) {
		if(x & (1<<j))
			break;
	}
	ASSERT( (j == 4) || (j == 0) ); /* for now */
	if( (j != 4) && (j != 0) ) {
		printf("ASSERTION ERROR: tp_mask_to_num: x 0x%x j %d\n",
			x, j);
	}
	IFTRACE(D_TPINPUT)
		tptrace(TPPTmisc, "tp_mask_to_num(x) returns j", x, j, 0, 0);
	ENDTRACE
	IFDEBUG(D_TPINPUT)
		printf("tp_mask_to_num(0x%x) returns 0x%x\n", x, j);
	ENDDEBUG
	return j;
}

static 
copyQOSparms(src, dst)
	struct tp_conn_param *src, *dst;
{
	/* copy all but the bits stuff at the end */
#define COPYSIZE (12 * sizeof(short))

	bcopy((caddr_t)src, (caddr_t)dst, COPYSIZE);
	dst->p_tpdusize = src->p_tpdusize;
	dst->p_ack_strat = src->p_ack_strat;
	dst->p_rx_strat = src->p_rx_strat;
#undef COPYSIZE
}

/*
 * CALLED FROM:
 *  tp_usrreq on PRU_CONNECT and tp_input on receipt of CR
 *	
 * FUNCTION and ARGUMENTS:
 * 	route directly to x.25 if the address is type 37 - GROT.
 *  furthermore, let TP0 handle only type-37 addresses
 *
 *	Since this assumes that its address argument is in a mbuf, the
 *	parameter was changed to reflect this assumtion. This also
 *	implies that an mbuf must be allocated when this is
 *	called from tp_input
 *	
 * RETURNS:
 *	errno value	 : 
 *	EAFNOSUPPORT if can't find an nl_protosw for x.25 (really could panic)
 *	ECONNREFUSED if trying to run TP0 with non-type 37 address
 *  possibly other E* returned from cons_netcmd()
 * NOTE:
 *  Would like to eliminate as much of this as possible -- 
 *  only one set of defaults (let the user set the parms according
 *  to parameters provided in the directory service).
 *  Left here for now 'cause we don't yet have a clean way to handle
 *  it on the passive end.
 */
int
tp_route_to( m, tpcb, channel)
	struct mbuf					*m;
	register struct tp_pcb		*tpcb;
	u_int 						channel;
{
	register struct sockaddr_iso *siso;	/* NOTE: this may be a sockaddr_in */
	extern struct tp_conn_param tp_conn_param[];
	int error = 0;
	int	vc_to_kill = 0; /* kludge */

	siso = mtod(m, struct sockaddr_iso *);
	IFTRACE(D_CONN)
		tptraceTPCB(TPPTmisc, 
		"route_to: so  afi netservice class",
		tpcb->tp_sock, siso->siso_addr.isoa_genaddr[0], tpcb->tp_netservice,
			tpcb->tp_class);
	ENDTRACE
	IFDEBUG(D_CONN)
		printf("tp_route_to( m x%x, channel 0x%x, tpcb 0x%x netserv 0x%x)\n", 
			m, channel, tpcb, tpcb->tp_netservice);
		printf("m->mlen x%x, m->m_data:\n", m->m_len);
		dump_buf(mtod(m, caddr_t), m->m_len);
	ENDDEBUG
	if( siso->siso_family != tpcb->tp_domain ) {
		error = EAFNOSUPPORT;
		goto done;
	}
	IFDEBUG(D_CONN)
		printf("tp_route_to  calling nlp_pcbconn, netserv %d\n",
			tpcb->tp_netservice);
	ENDDEBUG
	error = (tpcb->tp_nlproto->nlp_pcbconn)(tpcb->tp_sock->so_pcb, m);
	if( error )
		goto done;

	{
		register int save_netservice = tpcb->tp_netservice;

		switch(tpcb->tp_netservice) {
		case ISO_COSNS:
		case ISO_CLNS:
			/* This is a kludge but seems necessary so the passive end
			 * can get long enough timers. sigh.
			if( siso->siso_addr.osinet_idi[1] == (u_char)IDI_OSINET )
			 */
#define	IDI_OSINET	0x0004	/* bcd of "0004" */	
			if( siso->siso_addr.isoa_genaddr[2] == (char)IDI_OSINET ) {
				if( tpcb->tp_dont_change_params == 0) {
					copyQOSparms( &tp_conn_param[ISO_COSNS],
							&tpcb->_tp_param);
				}
				tpcb->tp_flags |= TPF_NLQOS_PDN;
			}
			/* drop through to IN_CLNS*/
		case IN_CLNS:
			if (iso_localifa(siso))
				tpcb->tp_flags |= TPF_PEER_ON_SAMENET;
			if( (tpcb->tp_class & TP_CLASS_4)==0 ) {
				error = EPROTOTYPE;
				break;
			} 
			tpcb->tp_class = TP_CLASS_4;  /* IGNORE dont_change_parms */
			break;

		case ISO_CONS:
#if NARGOXTWENTYFIVE > 0
			tpcb->tp_flags |= TPF_NLQOS_PDN;
			if( tpcb->tp_dont_change_params == 0 ) {
				copyQOSparms( &tp_conn_param[ISO_CONS],
							&tpcb->_tp_param);
			}
			/*
			 * for use over x.25 really need a small receive window,
			 * need to start slowly, need small max negotiable tpdu size,
			 * and need to use the congestion window to the max
			 * IGNORES tp_dont_change_params for these!
			 */
			if( tpcb->tp_sock->so_snd.sb_hiwat > 512 ) {
				(void) soreserve(tpcb->tp_sock, 512, 512 );/* GAG */
			}
			tpcb->tp_rx_strat =  TPRX_USE_CW;

			if( (tpcb->tp_nlproto != &nl_protosw[ISO_CONS]) ) {
				/* if the listener was restricting us to clns,
				 * ( we never get here if the listener isn't af_iso )
				 * refuse the connection :
				 * but we don't have a way to restrict thus - it's
				 * utterly permissive.
					if(channel)  {
						(void) cons_netcmd(CONN_REFUSE, tpcb->tp_npcb, 
								channel, tpcb->tp_class == TP_CLASS_4);
						error = EPFNOSUPPORT;
						goto done;
					}
				 */
				IFDEBUG(D_CONN)
					printf(
					"tp_route_to( CHANGING nlproto old 0x%x new 0x%x)\n", 
							tpcb->tp_nlproto , &nl_protosw[ISO_CONS]);
				ENDDEBUG
				tpcb->tp_nlproto = &nl_protosw[ISO_CONS];
			}
			/* Now we've got the right nl_protosw. 
			 * If we already have a channel (we were called from tp_input())
			 * tell cons that we'll hang onto this channel.
			 * If we don't already have one (we were called from usrreq())
			 * -and if it's TP0 open a net connection and wait for it to finish.
			 */
			if( channel ) {
				error = cons_netcmd( CONN_CONFIRM, tpcb->tp_npcb, 
								channel, tpcb->tp_class == TP_CLASS_4);
				vc_to_kill ++;
			} else if( tpcb->tp_class != TP_CLASS_4 /* class 4 only */) {
				/* better open vc if any possibility of ending up 
				 * in non-multiplexing class
				 */
				error = cons_openvc(tpcb->tp_npcb, siso, tpcb->tp_sock);
				vc_to_kill ++;
			}
			/* class 4 doesn't need to open a vc now - may use one already 
			 * opened or may open one only when it sends a pkt.
			 */
#else NARGOXTWENTYFIVE > 0
			error = ECONNREFUSED;
#endif NARGOXTWENTYFIVE > 0
			break;
		default:
			error = EPROTOTYPE;
		}

		ASSERT( save_netservice == tpcb->tp_netservice);
	}
	if( error && vc_to_kill ) {
		tp_netcmd( tpcb, CONN_CLOSE);
		goto done;
	} 
	{	/* start with the global rtt, rtv stats */
		register int i =
		   (int) tpcb->tp_flags & (TPF_PEER_ON_SAMENET | TPF_NLQOS_PDN);

		tpcb->tp_rtt = tp_stat.ts_rtt[i];
		tpcb->tp_rtv = tp_stat.ts_rtv[i];
	}
done:
	IFDEBUG(D_CONN)
		printf("tp_route_to  returns 0x%x\n", error);
	ENDDEBUG
	IFTRACE(D_CONN)
		tptraceTPCB(TPPTmisc, "route_to: returns: error netserv class", error, 
			tpcb->tp_netservice, tpcb->tp_class, 0);
	ENDTRACE
	return error;
}

#ifdef TP_PERF_MEAS
/*
 * CALLED FROM:
 *  tp_ctloutput() when the user sets TPOPT_PERF_MEAS on
 *  and tp_newsocket() when a new connection is made from 
 *  a listening socket with tp_perf_on == true.
 * FUNCTION and ARGUMENTS:
 *  (tpcb) is the usual; this procedure gets a clear cluster mbuf for
 *  a tp_pmeas structure, and makes tpcb->tp_p_meas point to it.
 * RETURN VALUE:
 *  ENOBUFS if it cannot get a cluster mbuf.
 */

int 
tp_setup_perf(tpcb)
	register struct tp_pcb *tpcb;
{
	register struct mbuf *q;

	if( tpcb->tp_p_meas == 0 ) {
		MGET(q, M_WAITOK, MT_PCB);
		if (q == 0)
			return ENOBUFS;
		MCLGET(q, M_WAITOK);
		if ((q->m_flags & M_EXT) == 0) {
			(void) m_free(q);
			return ENOBUFS;
		}
		q->m_len = sizeof (struct tp_pmeas);
		tpcb->tp_p_mbuf = q;
		tpcb->tp_p_meas = mtod(q, struct tp_pmeas *);
		bzero( (caddr_t)tpcb->tp_p_meas, sizeof (struct tp_pmeas) );
		IFDEBUG(D_PERF_MEAS)
			printf(
			"tpcb 0x%x so 0x%x ref 0x%x tp_p_meas 0x%x tp_perf_on 0x%x\n", 
				tpcb, tpcb->tp_sock, tpcb->tp_lref, 
				tpcb->tp_p_meas, tpcb->tp_perf_on);
		ENDDEBUG
		tpcb->tp_perf_on = 1;
	}
	return 0;
}
#endif TP_PERF_MEAS

#ifdef ARGO_DEBUG
dump_addr (addr)
	register struct sockaddr *addr;
{
	switch( addr->sa_family ) {
		case AF_INET:
			dump_inaddr((struct sockaddr_in *)addr);
			break;
#ifdef ISO
		case AF_ISO:
			dump_isoaddr((struct sockaddr_iso *)addr);
			break;
#endif ISO
		default:
			printf("BAD AF: 0x%x\n", addr->sa_family);
			break;
	}
}

#define	MAX_COLUMNS	8
/*
 *	Dump the buffer to the screen in a readable format. Format is:
 *
 *		hex/dec  where hex is the hex format, dec is the decimal format.
 *		columns of hex/dec numbers will be printed, followed by the
 *		character representations (if printable).
 */
Dump_buf(buf, len)
caddr_t	buf;
int		len;
{
	int		i,j;

	printf("Dump buf 0x%x len 0x%x\n", buf, len);
	for (i = 0; i < len; i += MAX_COLUMNS) {
		printf("+%d:\t", i);
		for (j = 0; j < MAX_COLUMNS; j++) {
			if (i + j < len) {
				printf("%x/%d\t", buf[i+j]&0xff, buf[i+j]);
			} else {
				printf("	");
			}
		}

		for (j = 0; j < MAX_COLUMNS; j++) {
			if (i + j < len) {
				if (((buf[i+j]) > 31) && ((buf[i+j]) < 128))
					printf("%c", buf[i+j]&0xff);
				else
					printf(".");
			}
		}
		printf("\n");
	}
}


#endif ARGO_DEBUG

