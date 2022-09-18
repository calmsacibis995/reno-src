/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)w.c	5.24 (Berkeley) 7/27/90";
#endif not lint

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <utmp.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <machine/pte.h>
#include <sys/vm.h>
#include <sys/tty.h>
#include <nlist.h>
#include <kvm.h>
#include <ctype.h>
#include <paths.h>
#include <string.h>
#include <stdio.h>

char	*program;
int	ttywidth;		/* width of tty */
int	argwidth;		/* width of tty */
int	header = 1;		/* true if -h flag: don't print heading */
int	wcmd = 1;		/* true if this is w(1), and not uptime(1) */
int	nusers;			/* number of users logged in now */
char *	sel_user;		/* login of particular user selected */
time_t	now;			/* the current time of day */
struct	timeval boottime;
time_t	uptime;			/* time of last reboot & elapsed time since */
struct	utmp utmp;
struct	winsize ws;
int	sortidle;		/* sort bu idle time */


/*
 * One of these per active utmp entry.  
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;		/* dev_t of terminal */
	int	idle;		/* idle time of terminal in minutes */
	struct	proc *proc;	/* list of procs in foreground */
	char	*args;		/* arg list of interesting process */
} *ep, *ehead = NULL, **nextp = &ehead;

struct nlist nl[] = {
	{ "_boottime" },
#define X_BOOTTIME	0
#if defined(hp300)
	{ "_cn_tty" },
#define X_CNTTY		1
#endif
	{ "" },
};

#define USAGE "[ -hi ] [ user ]"
#define usage()	fprintf(stderr, "usage: %s: %s\n", program, USAGE)

main(argc, argv)
	char **argv;
{
	register int i;
	struct winsize win;
	register struct proc *p;
	struct eproc *e;
	struct stat *stp, *ttystat();
	FILE *ut;
	char *cp;
	int ch;
	extern char *optarg;
	extern int optind;
	char *strsave();

	program = argv[0];
	/*
	 * are we w(1) or uptime(1)
	 */
	if ((cp = rindex(program, '/')) || *(cp = program) == '-')
		cp++;
	if (*cp == 'u')
		wcmd = 0;

	while ((ch = getopt(argc, argv, "hiflsuw")) != EOF)
		switch((char)ch) {
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle++;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			error("[-flsuw] no longer supported");
			usage();
			exit(1);
		case '?':
		default:
			usage();
			exit(1);
		}
	argc -= optind;
	argv += optind;
	if (argc == 1) {
		sel_user = argv[0];
		argv++, argc--;
	}
	if (argc) {
		usage();
		exit(1);
	}

	if (header && kvm_nlist(nl) != 0) {
		error("can't get namelist");
		exit (1);
	}
	time(&now);
	ut = fopen(_PATH_UTMP, "r");
	while (fread(&utmp, sizeof(utmp), 1, ut)) {
		if (utmp.ut_name[0] == '\0')
			continue;
		nusers++;
		if (wcmd == 0 || (sel_user && 
		    strncmp(utmp.ut_name, sel_user, UT_NAMESIZE) != 0))
			continue;
		if ((ep = (struct entry *)
		     calloc(1, sizeof (struct entry))) == NULL) {
			error("out of memory");
			exit(1);
		}
		*nextp = ep;
		nextp = &(ep->next);
		bcopy(&utmp, &(ep->utmp), sizeof (struct utmp));
		stp = ttystat(ep->utmp.ut_line);
		ep->tdev = stp->st_rdev;
#if defined(hp300)
		/*
		 * XXX  If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			static dev_t cn_dev;

			if (nl[X_CNTTY].n_value) {
				struct tty cn_tty, *cn_ttyp;
				
				if (kvm_read(nl[X_CNTTY].n_value,
					     &cn_ttyp, sizeof (cn_ttyp)) > 0) {
					(void)kvm_read(cn_ttyp, &cn_tty,
						       sizeof (cn_tty));
					cn_dev = cn_tty.t_dev;
				}
				nl[X_CNTTY].n_value = 0;
			}
			ep->tdev = cn_dev;
		}
#endif
		ep->idle = ((now - stp->st_atime) + 30) / 60; /* secs->mins */
		if (ep->idle < 0)
			ep->idle = 0;
	}
	fclose(ut);

	if (header || wcmd == 0) {
		double	avenrun[3];
		int days, hrs, mins;

		/*
		 * Print time of day 
		 */
		fputs(attime(&now), stdout);
		/*
		 * Print how long system has been up.
		 * (Found by looking for "boottime" in kernel)
		 */
		(void)kvm_read((off_t)nl[X_BOOTTIME].n_value, &boottime, 
			sizeof (boottime));
		uptime = now - boottime.tv_sec;
		uptime += 30;
		days = uptime / (60*60*24);
		uptime %= (60*60*24);
		hrs = uptime / (60*60);
		uptime %= (60*60);
		mins = uptime / 60;

		printf("  up");
		if (days > 0)
			printf(" %d day%s,", days, days>1?"s":"");
		if (hrs > 0 && mins > 0) {
			printf(" %2d:%02d,", hrs, mins);
		} else {
			if (hrs > 0)
				printf(" %d hr%s,", hrs, hrs>1?"s":"");
			if (mins > 0)
				printf(" %d min%s,", mins, mins>1?"s":"");
		}

		/* Print number of users logged in to system */
		printf("  %d user%s", nusers, nusers>1?"s":"");

		/*
		 * Print 1, 5, and 15 minute load averages.
		 */
		printf(",  load average:");
		(void)getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));
		for (i = 0; i < (sizeof(avenrun)/sizeof(avenrun[0])); i++) {
			if (i > 0)
				printf(",");
			printf(" %.2f", avenrun[i]);
		}
		printf("\n");
		if (wcmd == 0)		/* if uptime(1) then done */
			exit(0);
#define HEADER	"USER    TTY FROM              LOGIN@  IDLE WHAT\n"
#define WUSED	(sizeof (HEADER) - sizeof ("WHAT\n"))
		printf(HEADER);
	}

	while ((p = kvm_nextproc()) != NULL) {
		if (p->p_stat == SZOMB || (p->p_flag & SCTTY) == 0)
			continue;
		e = kvm_geteproc(p);
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == e->e_tdev && e->e_pgid == e->e_tpgid) {
				/*
				 * Proc is in foreground of this terminal
				 */
				if (proc_compare(ep->proc, p))
					ep->proc = p;
				break;
			}
		}
	}
	if ((ioctl(1, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(2, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(0, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
	       ttywidth = 79;
        else
	       ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - WUSED;
	if (argwidth < 4)
		argwidth = 8;
	for (ep = ehead; ep != NULL; ep = ep->next) {
		ep->args = strsave(kvm_getargs(ep->proc, kvm_getu(ep->proc)));
		if (ep->args == NULL) {
			error("out of memory");
			exit(1);
		}
	}
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;
		
		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead; 
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}
			
	for (ep = ehead; ep != NULL; ep = ep->next) {
		printf("%-*.*s %-2.2s %-*.*s %s",
			UT_NAMESIZE, UT_NAMESIZE, ep->utmp.ut_name,
			strncmp(ep->utmp.ut_line, "tty", 3) == 0 ? 
				ep->utmp.ut_line+3 : ep->utmp.ut_line,
			UT_HOSTSIZE, UT_HOSTSIZE, *ep->utmp.ut_host ?
				ep->utmp.ut_host : "-",
			attime(&ep->utmp.ut_time));
		if (ep->idle >= 36 * 60)
			printf(" %ddays ", (ep->idle + 12 * 60) / (24 * 60));
		else
			prttime(ep->idle, " ");
		printf("%.*s\n", argwidth, ep->args);
	}
}

struct stat *
ttystat(line)
{
	static struct stat statbuf;
	char ttybuf[sizeof (_PATH_DEV) + UT_LINESIZE + 1];

	sprintf(ttybuf, "%s/%.*s", _PATH_DEV, UT_LINESIZE, line);
	(void) stat(ttybuf, &statbuf);

	return (&statbuf);
}

char *
strsave(cp)
	char *cp;
{
	register unsigned len;
	register char *dp;

	len = strlen(cp);
	dp = (char *)calloc(len+1, sizeof (char));
	(void) strcpy(dp, cp);
	return (dp);
}
/*
 * prttime prints a time in hours and minutes or minutes and seconds.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */
prttime(tim, tail)
	time_t tim;
	char *tail;
{

	if (tim >= 60) {
		printf(" %2d:", tim/60);
		tim %= 60;
		printf("%02d", tim);
	} else if (tim >= 0)
		printf("    %2d", tim);
	printf("%s", tail);
}

#include <varargs.h>

warning(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;

	fprintf(stderr, "%s: warning: ", program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

error(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;

	fprintf(stderr, "%s: ", program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

syserror(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;
	extern errno;

	fprintf(stderr, "%s: ", program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errno));
}
