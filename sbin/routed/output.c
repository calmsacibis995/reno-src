/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)output.c	5.14 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"

/*
 * Apply the function "f" to all non-passive
 * interfaces.  If the interface supports the
 * use of broadcasting use it, otherwise address
 * the output to the known router.
 */
toall(f, rtstate, skipif)
	int (*f)();
	int rtstate;
	struct interface *skipif;
{
	register struct interface *ifp;
	register struct sockaddr *dst;
	register int flags;
	extern struct interface *ifnet;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_flags & IFF_PASSIVE || ifp == skipif)
			continue;
		dst = ifp->int_flags & IFF_BROADCAST ? &ifp->int_broadaddr :
		      ifp->int_flags & IFF_POINTOPOINT ? &ifp->int_dstaddr :
		      &ifp->int_addr;
		flags = ifp->int_flags & IFF_INTERFACE ? MSG_DONTROUTE : 0;
		(*f)(dst, flags, ifp, rtstate);
	}
}

/*
 * Output a preformed packet.
 */
/*ARGSUSED*/
sendmsg(dst, flags, ifp, rtstate)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
	int rtstate;
{

	(*afswitch[dst->sa_family].af_output)(s, flags,
		dst, sizeof (struct rip));
	TRACE_OUTPUT(ifp, dst, sizeof (struct rip));
}

/*
 * Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
supply(dst, flags, ifp, rtstate)
	struct sockaddr *dst;
	int flags;
	register struct interface *ifp;
	int rtstate;
{
	register struct rt_entry *rt;
	register struct netinfo *n = msg->rip_nets;
	register struct rthash *rh;
	struct rthash *base = hosthash;
	int doinghost = 1, size;
	int (*output)() = afswitch[dst->sa_family].af_output;
	int (*sendroute)() = afswitch[dst->sa_family].af_sendroute;
	int npackets = 0;

	msg->rip_cmd = RIPCMD_RESPONSE;
	msg->rip_vers = RIPVERSION;
	bzero(msg->rip_res1, sizeof(msg->rip_res1));
again:
	for (rh = base; rh < &base[ROUTEHASHSIZ]; rh++)
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		/*
		 * Don't resend the information on the network
		 * from which it was received (unless sending
		 * in response to a query).
		 */
		if (ifp && rt->rt_ifp == ifp &&
		    (rt->rt_state & RTS_INTERFACE) == 0)
			continue;
		if (rt->rt_state & RTS_EXTERNAL)
			continue;
		/*
		 * For dynamic updates, limit update to routes
		 * with the specified state.
		 */
		if (rtstate && (rt->rt_state & rtstate) == 0)
			continue;
		/*
		 * Limit the spread of subnet information
		 * to those who are interested.
		 */
		if (doinghost == 0 && rt->rt_state & RTS_SUBNET) {
			if (rt->rt_dst.sa_family != dst->sa_family)
				continue;
			if ((*sendroute)(rt, dst) == 0)
				continue;
		}
		size = (char *)n - packet;
		if (size > MAXPACKETSIZE - sizeof (struct netinfo)) {
			TRACE_OUTPUT(ifp, dst, size);
			(*output)(s, flags, dst, size);
			/*
			 * If only sending to ourselves,
			 * one packet is enough to monitor interface.
			 */
			if (ifp && (ifp->int_flags &
			   (IFF_BROADCAST | IFF_POINTOPOINT | IFF_REMOTE)) == 0)
				return;
			n = msg->rip_nets;
			npackets++;
		}
		n->rip_dst = rt->rt_dst;
#if BSD < 198810
		if (sizeof(n->rip_dst.sa_family) > 1)	/* XXX */
		n->rip_dst.sa_family = htons(n->rip_dst.sa_family);
#else
#define osa(x) ((struct osockaddr *)(&(x)))
		osa(n->rip_dst)->sa_family = htons(n->rip_dst.sa_family);
#endif
		n->rip_metric = htonl(rt->rt_metric);
		n++;
	}
	if (doinghost) {
		doinghost = 0;
		base = nethash;
		goto again;
	}
	if (n != msg->rip_nets || (npackets == 0 && rtstate == 0)) {
		size = (char *)n - packet;
		TRACE_OUTPUT(ifp, dst, size);
		(*output)(s, flags, dst, size);
	}
}
