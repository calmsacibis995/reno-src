/*
 * Copyright (c) 1989 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)printf.c	5.9 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#define PF(f, func) { \
	if (fieldwidth) \
		if (precision) \
			(void)printf(f, fieldwidth, precision, func); \
		else \
			(void)printf(f, fieldwidth, func); \
	else if (precision) \
		(void)printf(f, precision, func); \
	else \
		(void)printf(f, func); \
}

char **gargv;

main(argc, argv)
	int argc;
	char **argv;
{
	static char *skip1, *skip2;
	register char *format, *fmt, *start;
	register int end, fieldwidth, precision;
	char convch, nextch, *getstr(), *index(), *mklong();
	double getdouble();
	long getlong();

	if (argc < 2) {
		fprintf(stderr, "usage: printf format [arg ...]\n");
		exit(1);
	}

	/*
	 * Basic algorithm is to scan the format string for conversion
	 * specifications -- once one is found, find out if the field
	 * width or precision is a '*'; if it is, gather up value.  Note,
	 * format strings are reused as necessary to use up the provided
	 * arguments, arguments of zero/null string are provided to use
	 * up the format string.
	 */
	skip1 = "#-+ 0";
	skip2 = "*0123456789";

	escape(fmt = format = *++argv);		/* backslash interpretation */
	gargv = ++argv;
	for (;;) {
		end = 0;
		/* find next format specification */
next:		for (start = fmt;; ++fmt) {
			if (!*fmt) {
				/* avoid infinite loop */
				if (end == 1) {
					fprintf(stderr,
					    "printf: missing format character.\n");
					exit(1);
				}
				end = 1;
				if (fmt > start)
					(void)printf("%s", start);
				if (!*gargv)
					exit(0);
				fmt = format;
				goto next;
			}
			/* %% prints a % */
			if (*fmt == '%') {
				if (*++fmt != '%')
					break;
				*fmt++ = '\0';
				(void)printf("%s", start);
				goto next;
			}
		}

		/* skip to field width */
		for (; index(skip1, *fmt); ++fmt);
		fieldwidth = *fmt == '*' ? getint() : 0;

		/* skip to possible '.', get following precision */
		for (; index(skip2, *fmt); ++fmt);
		if (*fmt == '.')
			++fmt;
		precision = *fmt == '*' ? getint() : 0;

		/* skip to conversion char */
		for (; index(skip2, *fmt); ++fmt);
		if (!*fmt) {
			fprintf(stderr, "printf: missing format character.\n");
			exit(1);
		}

		convch = *fmt;
		nextch = *++fmt;
		*fmt = '\0';
		switch(convch) {
		case 'c': {
			char p = getchr();
			PF(start, p);
			break;
		}
		case 's': {
			char *p = getstr();
			PF(start, p);
			break;
		}
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
			char *f = mklong(start, convch);
			long p = getlong();
			PF(f, p);
			break;
		}
		case 'e': case 'E': case 'f': case 'g': case 'G': {
			double p = getdouble();
			PF(start, p);
			break;
		}
		default:
			fprintf(stderr, "printf: illegal format character.\n");
			exit(1);
		}
		*fmt = nextch;
	}
	/* NOTREACHED */
}

char *
mklong(str, ch)
	char *str, ch;
{
	int len;
	char *copy, *malloc();

	len = strlen(str) + 2;
	if (!(copy = malloc((u_int)len))) {	/* never freed; XXX */
		fprintf(stderr, "printf: out of memory.\n");
		exit(1);
	}
	bcopy(str, copy, len - 3);
	copy[len - 3] = 'l';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return(copy);
}

escape(fmt)
	register char *fmt;
{
	register char *store;
	register int value, c;

	for (store = fmt; c = *fmt; ++fmt, ++store) {
		if (c != '\\') {
			*store = c;
			continue;
		}
		switch (*++fmt) {
		case '\0':		/* EOS, user error */
			*store = '\\';
			*++store = '\0';
			return;
		case '\\':		/* backslash */
		case '\'':		/* single quote */
			*store = *fmt;
			break;
		case 'a':		/* bell/alert */
			*store = '\7';
			break;
		case 'b':		/* backspace */
			*store = '\b';
			break;
		case 'f':		/* form-feed */
			*store = '\f';
			break;
		case 'n':		/* newline */
			*store = '\n';
			break;
		case 'r':		/* carriage-return */
			*store = '\r';
			break;
		case 't':		/* horizontal tab */
			*store = '\t';
			break;
		case 'v':		/* vertical tab */
			*store = '\13';
			break;
					/* octal constant */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			for (c = 3, value = 0;
			    c-- && *fmt >= '0' && *fmt <= '7'; ++fmt) {
				value <<= 3;
				value += *fmt - '0';
			}
			--fmt;
			*store = value;
			break;
		default:
			*store = *fmt;
			break;
		}
	}
	*store = '\0';
}

getchr()
{
	if (!*gargv)
		return((int)'\0');
	return((int)**gargv++);
}

char *
getstr()
{
	if (!*gargv)
		return("");
	return(*gargv++);
}

static char *number = "+-.0123456789";
getint()
{
	if (!*gargv)
		return(0);
	if (index(number, **gargv))
		return(atoi(*gargv++));
	return(asciicode());
}

long
getlong()
{
	long atol();

	if (!*gargv)
		return((long)0);
	if (index(number, **gargv))
		return(strtol(*gargv++, (char **)NULL, 0));
	return((long)asciicode());
}

double
getdouble()
{
	double atof();

	if (!*gargv)
		return((double)0);
	if (index(number, **gargv))
		return(atof(*gargv++));
	return((double)asciicode());
}

asciicode()
{
	register char ch;

	ch = **gargv;
	if (ch == '\'' || ch == '"')
		ch = (*gargv)[1];
	++gargv;
	return(ch);
}
