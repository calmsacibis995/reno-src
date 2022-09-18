/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)if_ecreg.h	7.4 (Berkeley) 6/28/90
 */

/*
 * 3Com Ethernet controller registers.
 */
struct ecdevice {
	short	ec_rcr;		/* Receive Control Register */
	short	ec_xcr;		/* Transmit Control Register */
};

/*
 * Control and status bits -- rcr
 */
#define	EC_SPIE		0x8000		/* set parity interrupt enable */
#define	EC_ASTEP	0x4000		/* increment address counter */
#define	EC_AROM		0x2000		/* 1: Use address ROM, 0: use RAM */
#define	EC_PE		0x2000		/* Parity error */
#define	EC_AWCLK	0x1000		/* address write clock bit */
#define	EC_PIE		0x1000		/* Parity interrupt enable (read) */
#define	EC_ADATA	0x0f00		/* address/filtering */
#define	EC_RDONE	0x0080		/* receive done */
#define	EC_MDISAB	0x0080		/* memory disable */
#define	EC_RINTEN	0x0040		/* receive interrupt enable */
#define	EC_RCLR		0x0020		/* clear RDONE bit */
#define	EC_RWBN		0x0010		/* submit buffer for receive */
#define	EC_RBN		0x000f		/* buffer number */

#define	EC_RBITS	"\10\16PE\15PIE\10RDONE\7RINTEN"

/*
 * Control and status bits -- xcr
 */
#define	EC_JAM		0x8000		/* collision dectected */
#define	EC_JINTEN	0x4000		/* collision interrupt enable */
#define	EC_JCLR		0x2000		/* clear collision detect */
#define	EC_UECLR	0x0100		/* reset controller */
#define	EC_XDONE	0x0080		/* transmit done */
#define	EC_XINTEN	0x0040		/* transmit interrupt enable */
#define	EC_XCLR		0x0020		/* clear XDONE bit */
#define	EC_XWBN		0x0010		/* submit buffer for transmit */
#define	EC_XBN		0x000f		/* buffer number */

#define	EC_XBITS	"\10\20JAM\17JINTEN\10XDONE\7XINTEN"

/*
 * Useful combinations
 */
#define	EC_READ		(0x600|EC_RINTEN|EC_RWBN)
#define	EC_MULTI	(0x700|EC_RINTEN|EC_RWBN)
#define EC_PROMISC	(0x000|EC_RINTEN|EC_RWBN)
#define	EC_WRITE	(EC_JINTEN|EC_XINTEN|EC_XWBN)
#define	EC_CLEAR	(EC_JINTEN|EC_XINTEN|EC_JCLR)

/*
 * Buffer number definitions
 */
#define	ECTBF		0		/* Buffer for transmit */
#define	ECRLBF		1		/* First buffer for receive */
#define	ECRHBF		15		/* Last buffer for receive */

#define	ECRDOFF		528		/* Packet offset in read buffer */
