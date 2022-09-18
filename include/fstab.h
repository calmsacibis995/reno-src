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
 *
 *	@(#)fstab.h	5.10 (Berkeley) 6/1/90
 */

/*
 * File system table, see fstab(5).
 *
 * Used by dump, mount, umount, swapon, fsck, df, ...
 *
 * For ufs fs_spec field is the block special name.  Programs that want to
 * use the character special name must create that name by prepending a 'r'
 * after the right most slash.  Quota files are always named "quotas", so
 * if type is "rq", then use concatenation of fs_file and "quotas" to locate
 * quota file.
 */
#define	_PATH_FSTAB	"/etc/fstab"
#define	FSTAB		"/etc/fstab"	/* deprecated */

#define	FSTAB_RW	"rw"		/* read/write device */
#define	FSTAB_RQ	"rq"		/* read/write with quotas */
#define	FSTAB_RO	"ro"		/* read-only device */
#define	FSTAB_SW	"sw"		/* swap device */
#define	FSTAB_XX	"xx"		/* ignore totally */

struct fstab {
	char	*fs_spec;		/* block special device name */
	char	*fs_file;		/* file system path prefix */
	char	*fs_vfstype;		/* File system type, ufs, nfs */
	char	*fs_mntops;		/* Mount options ala -o */
	char	*fs_type;		/* FSTAB_* from fs_mntops */
	int	fs_freq;		/* dump frequency, in days */
	int	fs_passno;		/* pass number on parallel dump */
};

#if __STDC__ || c_plusplus
extern struct fstab *getfsent(void);
extern struct fstab *getfsspec(const char *);
extern struct fstab *getfsfile(const char *);
extern int setfsent(void);
extern void endfsent(void);
#else
extern struct fstab *getfsent();
extern struct fstab *getfsspec();
extern struct fstab *getfsfile();
extern int setfsent();
extern void endfsent();
#endif
