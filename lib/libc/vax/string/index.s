/*
 * Copyright (c) 1980 Regents of the University of California.
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

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "@(#)index.s	5.6 (Berkeley) 6/1/90"
#endif /* LIBC_SCCS and not lint */

/*
 * Find the first occurence of c in the string cp.
 * Return pointer to match or null pointer.
 *
 * char *
 * index(cp, c)
 *	char *cp, c;
 */
#include "DEFS.h"

ENTRY(index, 0)
	movq	4(ap),r1	# r1 = cp; r2 = c
	tstl	r2		# check for special case c == '\0'
	bneq	2f
1:
	locc	$0,$65535,(r1)	# just find end of string
	beql	1b		# still looking
	movl	r1,r0		# found it
	ret
2:
	moval	tbl,r3		# r3 = address of table
	bbss	$0,(r3),5f	# insure not reentering
	movab	(r3)[r2],r5	# table entry for c
	incb	(r5)
	movzwl	$65535,r4	# fast access
3:
	scanc	r4,(r1),(r3),$1	# look for c or '\0'
	beql	3b		# still looking
	movl	r1,r0		# return pointer to char
	tstb	(r0)		#    if have found '\0'
	bneq	4f
	clrl	r0		# else return 0
4:
	clrb	(r5)		# clean up table
	clrb	(r3)
	ret

	.data
tbl:	.space	256
	.text

/*
 * Reentrant, but slower version of index
 */
5:
	movl	r1,r3
6:
	locc	$0,$65535,(r3)	# look for '\0'
	bneq	7f
	locc	r2,$65535,(r3)	# look for c
	bneq	8f
	movl	r1,r3		# reset pointer and ...
	jbr	6b		# ... try again
7:
	subl3	r3,r1,r4	# length of short block
	incl	r4		# +1 for '\0'
	locc	r2,r4,(r3)	# look for c
	bneq	8f
	ret
8:
	movl	r1,r0		# return pointer to char
	ret
