/*
 * Copyright (c) 1982, 1986, 1989, 1990 Regents of the University of California.
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
 *	@(#)uipc_syscalls.c	7.20 (Berkeley) 6/30/90
 */

#include "param.h"
#include "user.h"
#include "proc.h"
#include "file.h"
#include "buf.h"
#include "malloc.h"
#include "mbuf.h"
#include "protosw.h"
#include "socket.h"
#include "socketvar.h"
#ifdef KTRACE
#include "ktrace.h"
#endif

/*
 * System call interface to the socket abstraction.
 */

struct	file *getsock();
extern	struct fileops socketops;

/* ARGSUSED */
socket(p, uap, retval)
	struct proc *p;
	register struct args {
		int	domain;
		int	type;
		int	protocol;
	} *uap;
	int *retval;
{
	struct socket *so;
	struct file *fp;
	int fd, error;

	if (error = falloc(&fp, &fd))
		return (error);
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	if (error = socreate(uap->domain, &so, uap->type, uap->protocol)) {
		u.u_ofile[fd] = 0;
		crfree(fp->f_cred);
		fp->f_count = 0;
	} else {
		fp->f_data = (caddr_t)so;
		*retval = fd;
	}
	return (error);
}

/* ARGSUSED */
bind(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	name;
		int	namelen;
	} *uap;
	int *retval;
{
	register struct file *fp;
	struct mbuf *nam;
	int error;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	if (error = sockargs(&nam, uap->name, uap->namelen, MT_SONAME))
		return (error);
	error = sobind((struct socket *)fp->f_data, nam);
	m_freem(nam);
	return (error);
}

/* ARGSUSED */
listen(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		int	backlog;
	} *uap;
	int *retval;
{
	register struct file *fp;
	int error;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	return (solisten((struct socket *)fp->f_data, uap->backlog));
}

#ifdef COMPAT_43
accept(p, uap, retval)
	struct proc *p;
	struct args {
		int	s;
		caddr_t	name;
		int	*anamelen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 0;
	return (accept1(p, uap, retval));
}

oaccept(p, uap, retval)
	struct proc *p;
	struct args {
		int	s;
		caddr_t	name;
		int	*anamelen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 1;
	return (accept1(p, uap, retval));
}
#else /* COMPAT_43 */

#define	accept1	accept
#endif

/* ARGSUSED */
accept1(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	name;
		int	*anamelen;
#ifdef COMPAT_43
		int	compat_43;
#endif
	} *uap;
	int *retval;
{
	struct file *fp;
	struct mbuf *nam;
	int namelen, error, s;
	register struct socket *so;

	if (uap->name && (error = copyin((caddr_t)uap->anamelen,
	    (caddr_t)&namelen, sizeof (namelen))))
		return (error);
	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	s = splnet();
	so = (struct socket *)fp->f_data;
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		splx(s);
		return (EINVAL);
	}
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		splx(s);
		return (EWOULDBLOCK);
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		    netcon, 0)) {
			splx(s);
			return (error);
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		splx(s);
		return (error);
	}
	if (error = falloc(&fp, retval)) {
		splx(s);
		return (error);
	}
	{ struct socket *aso = so->so_q;
	  if (soqremque(aso, 1) == 0)
		panic("accept");
	  so = aso;
	}
	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = FREAD|FWRITE;
	fp->f_ops = &socketops;
	fp->f_data = (caddr_t)so;
	nam = m_get(M_WAIT, MT_SONAME);
	(void) soaccept(so, nam);
	if (uap->name) {
#ifdef COMPAT_43
		if (uap->compat_43)
			mtod(nam, struct osockaddr *)->sa_family =
			    mtod(nam, struct sockaddr *)->sa_family;
#endif
		if (namelen > nam->m_len)
			namelen = nam->m_len;
		/* SHOULD COPY OUT A CHAIN HERE */
		if ((error = copyout(mtod(nam, caddr_t), (caddr_t)uap->name,
		    (u_int)namelen)) == 0)
			error = copyout((caddr_t)&namelen,
			    (caddr_t)uap->anamelen, sizeof (*uap->anamelen));
	}
	m_freem(nam);
	splx(s);
	return (error);
}

/* ARGSUSED */
connect(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	name;
		int	namelen;
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct socket *so;
	struct mbuf *nam;
	int error, s;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
		return (EALREADY);
	if (error = sockargs(&nam, uap->name, uap->namelen, MT_SONAME))
		return (error);
	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		m_freem(nam);
		return (EINPROGRESS);
	}
	s = splnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
		if (error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		    netcon, 0))
			break;
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	splx(s);
bad:
	so->so_state &= ~SS_ISCONNECTING;
	m_freem(nam);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

/* ARGSUSED */
socketpair(p, uap, retval)
	struct proc *p;
	register struct args {
		int	domain;
		int	type;
		int	protocol;
		int	*rsv;
	} *uap;
	int retval[];
{
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, sv[2];

	if (error = socreate(uap->domain, &so1, uap->type, uap->protocol))
		return (error);
	if (error = socreate(uap->domain, &so2, uap->type, uap->protocol))
		goto free1;
	if (error = falloc(&fp1, &fd))
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = (caddr_t)so1;
	if (error = falloc(&fp2, &fd))
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = (caddr_t)so2;
	sv[1] = fd;
	if (error = soconnect2(so1, so2))
		goto free4;
	if (uap->type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 if (error = soconnect2(so2, so1))
			goto free4;
	}
	error = copyout((caddr_t)sv, (caddr_t)uap->rsv, 2 * sizeof (int));
	retval[0] = sv[0];		/* XXX ??? */
	retval[1] = sv[1];		/* XXX ??? */
	return (error);
free4:
	crfree(fp2->f_cred);
	fp2->f_count = 0;
	u.u_ofile[sv[1]] = 0;
free3:
	crfree(fp1->f_cred);
	fp1->f_count = 0;
	u.u_ofile[sv[0]] = 0;
free2:
	(void)soclose(so2);
free1:
	(void)soclose(so1);
	return (error);
}

/* ARGSUSED */
sendto(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
		caddr_t	to;
		int	tolen;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = uap->to;
	msg.msg_namelen = uap->tolen;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_43
	msg.msg_flags = 0;
#endif
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	return (sendit(uap->s, &msg, uap->flags, retval));
}

#ifdef COMPAT_43
/* ARGSUSED */
osend(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = 0;
	return (sendit(uap->s, &msg, uap->flags, retval));
}

#define MSG_COMPAT	0x8000
/* ARGSUSED */
osendmsg(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	msg;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin(uap->msg, (caddr_t)&msg, sizeof (struct omsghdr)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV, 
		      M_WAITOK);
	} else
		iov = aiov;
	if (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	msg.msg_flags = MSG_COMPAT;
	msg.msg_iov = iov;
	error = sendit(uap->s, &msg, uap->flags, retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

/* ARGSUSED */
sendmsg(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	msg;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin(uap->msg, (caddr_t)&msg, sizeof (msg)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else
		iov = aiov;
	if (msg.msg_iovlen &&
	    (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)))))
		goto done;
	msg.msg_iov = iov;
#ifdef COMPAT_43
	msg.msg_flags = 0;
#endif
	error = sendit(uap->s, &msg, uap->flags, retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

sendit(s, mp, flags, retsize)
	int s;
	register struct msghdr *mp;
	int flags, *retsize;
{
	register struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	struct mbuf *to, *control;
	int len, error;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	fp = getsock(s, &error);
	if (fp == 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if (iov->iov_len < 0)
			return (EINVAL);
		if ((auio.uio_resid += iov->iov_len) < 0)
			return (EINVAL);
	}
	if (mp->msg_name) {
		if (error = sockargs(&to, mp->msg_name, mp->msg_namelen,
		    MT_SONAME))
			return (error);
	} else
		to = 0;
	if (mp->msg_control) {
		if (mp->msg_controllen < sizeof(struct cmsghdr)
#ifdef COMPAT_43
		    && mp->msg_flags != MSG_COMPAT
#endif
		) {
			error = EINVAL;
			goto bad;
		}
		if (error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL))
			goto bad;
#ifdef COMPAT_43
		if (mp->msg_flags == MSG_COMPAT) {
			register struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAIT);
			if (control == 0) {
				error = ENOBUFS;
				goto bad;
			} else {
				cm = mtod(control, struct cmsghdr *);
				cm->cmsg_len = control->m_len;
				cm->cmsg_level = SOL_SOCKET;
				cm->cmsg_type = SCM_RIGHTS;
			}
		}
#endif
	} else
		control = 0;
#ifdef KTRACE
	if (KTRPOINT(u.u_procp, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	if (error = sosend((struct socket *)fp->f_data, to, &auio,
	    (struct mbuf *)0, control, flags)) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(u.u_procp, SIGPIPE);
	}
	if (error == 0)
		*retsize = len - auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(u.u_procp->p_tracep, s, UIO_WRITE,
				ktriov, *retsize, error);
		FREE(ktriov, M_TEMP);
	}
#endif
bad:
	if (to)
		m_freem(to);
	return (error);
}

#ifdef COMPAT_43
orecvfrom(p, uap, retval)
	struct proc *p;
	struct args {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
		caddr_t	from;
		int	*fromlenaddr;
	} *uap;
	int *retval;
{

	uap->flags |= MSG_COMPAT;
	return (recvfrom(p, uap, retval));
}
#endif

/* ARGSUSED */
recvfrom(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
		caddr_t	from;
		int	*fromlenaddr;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (uap->fromlenaddr) {
		if (error = copyin((caddr_t)uap->fromlenaddr,
		    (caddr_t)&msg.msg_namelen, sizeof (msg.msg_namelen)))
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = uap->from;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	return (recvit(uap->s, &msg, (caddr_t)uap->fromlenaddr, retval));
}

#ifdef COMPAT_43
/* ARGSUSED */
orecv(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		caddr_t	buf;
		int	len;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->len;
	msg.msg_control = 0;
	msg.msg_flags = uap->flags;
	return (recvit(uap->s, &msg, (caddr_t)0, retval));
}

/*
 * Old recvmsg.  This code takes advantage of the fact that the old msghdr
 * overlays the new one, missing only the flags, and with the (old) access
 * rights where the control fields are now.
 */
/* ARGSUSED */
orecvmsg(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		struct	omsghdr *msg;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	if (error = copyin((caddr_t)uap->msg, (caddr_t)&msg,
	    sizeof (struct omsghdr)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		      sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		      M_WAITOK);
	} else
		iov = aiov;
	msg.msg_flags = uap->flags | MSG_COMPAT;
	if (error = copyin((caddr_t)msg.msg_iov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	msg.msg_iov = iov;
	error = recvit(uap->s, &msg, (caddr_t)&uap->msg->msg_namelen, retval);

	if (msg.msg_controllen && error == 0)
		error = copyout((caddr_t)&msg.msg_controllen,
		    (caddr_t)&uap->msg->msg_accrightslen, sizeof (int));
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}
#endif

/* ARGSUSED */
recvmsg(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		struct	msghdr *msg;
		int	flags;
	} *uap;
	int *retval;
{
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	register int error;

	if (error = copyin((caddr_t)uap->msg, (caddr_t)&msg, sizeof (msg)))
		return (error);
	if ((u_int)msg.msg_iovlen >= UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen >= UIO_MAXIOV)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else
		iov = aiov;
#ifdef COMPAT_43
	msg.msg_flags = uap->flags &~ MSG_COMPAT;
#else
	msg.msg_flags = uap->flags;
#endif
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	if (error = copyin((caddr_t)uiov, (caddr_t)iov,
	    (unsigned)(msg.msg_iovlen * sizeof (struct iovec))))
		goto done;
	if ((error = recvit(uap->s, &msg, (caddr_t)0, retval)) == 0) {
		msg.msg_iov = uiov;
		error = copyout((caddr_t)&msg, (caddr_t)uap->msg, sizeof(msg));
	}
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

recvit(s, mp, namelenp, retsize)
	int s;
	register struct msghdr *mp;
	caddr_t namelenp;
	int *retsize;
{
	register struct file *fp;
	struct uio auio;
	register struct iovec *iov;
	register int i;
	int len, error;
	struct mbuf *from = 0, *control = 0;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	fp = getsock(s, &error);
	if (fp == 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if (iov->iov_len < 0)
			return (EINVAL);
		if ((auio.uio_resid += iov->iov_len) < 0)
			return (EINVAL);
	}
#ifdef KTRACE
	if (KTRPOINT(u.u_procp, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	if (error = soreceive((struct socket *)fp->f_data, &from, &auio,
	    (struct mbuf **)0, &control, &mp->msg_flags)) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(u.u_procp->p_tracep, s, UIO_READ,
				ktriov, len - auio.uio_resid, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	if (error)
		goto out;
	*retsize = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || from == 0)
			len = 0;
		else {
#ifdef COMPAT_43
			if (mp->msg_flags & MSG_COMPAT)
				mtod(from, struct osockaddr *)->sa_family =
				    mtod(from, struct sockaddr *)->sa_family;
#endif
			if (len > from->m_len)
				len = from->m_len;
			/* else if len < from->m_len ??? */
			if (error = copyout(mtod(from, caddr_t),
			    (caddr_t)mp->msg_name, (unsigned)len))
				goto out;
		}
		mp->msg_namelen = len;
		if (namelenp &&
		    (error = copyout((caddr_t)&len, namelenp, sizeof (int)))) {
#ifdef COMPAT_43
			if (mp->msg_flags & MSG_COMPAT)
				error = 0;	/* old recvfrom didn't check */
			else
#endif
			goto out;
		}
	}
	if (mp->msg_control) {
#ifdef	COMPAT_43
		/*
		 * We assume that old recvmsg calls won't receive access
		 * rights and other control info, esp. as control info
		 * is always optional and those options didn't exist in 4.3.
		 * If we receive rights, trim the cmsghdr; anything else
		 * is tossed.
		 */
		if (control && mp->msg_flags & MSG_COMPAT) {
			if (mtod(control, struct cmsghdr *)->cmsg_level !=
			    SOL_SOCKET ||
			    mtod(control, struct cmsghdr *)->cmsg_type !=
			    SCM_RIGHTS) {
				mp->msg_controllen = 0;
				goto out;
			}
			control->m_len -= sizeof (struct cmsghdr);
			control->m_data += sizeof (struct cmsghdr);
		}
#endif
		len = mp->msg_controllen;
		if (len <= 0 || control == 0)
			len = 0;
		else {
			if (len >= control->m_len)
				len = control->m_len;
			else
				mp->msg_flags |= MSG_CTRUNC;
			error = copyout((caddr_t)mtod(control, caddr_t),
			    (caddr_t)mp->msg_control, (unsigned)len);
		}
		mp->msg_controllen = len;
	}
out:
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);
	return (error);
}

/* ARGSUSED */
shutdown(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		int	how;
	} *uap;
	int *retval;
{
	struct file *fp;
	int error;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	return (soshutdown((struct socket *)fp->f_data, uap->how));
}

/* ARGSUSED */
setsockopt(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		int	level;
		int	name;
		caddr_t	val;
		int	valsize;
	} *uap;
	int *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	if (uap->valsize > MLEN)
		return (EINVAL);
	if (uap->val) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		if (error = copyin(uap->val, mtod(m, caddr_t),
		    (u_int)uap->valsize)) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = uap->valsize;
	}
	return (sosetopt((struct socket *)fp->f_data, uap->level,
	    uap->name, m));
}

/* ARGSUSED */
getsockopt(p, uap, retval)
	struct proc *p;
	register struct args {
		int	s;
		int	level;
		int	name;
		caddr_t	val;
		int	*avalsize;
	} *uap;
	int *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int valsize, error;

	fp = getsock(uap->s, &error);
	if (fp == 0)
		return (error);
	if (uap->val) {
		if (error = copyin((caddr_t)uap->avalsize, (caddr_t)&valsize,
		    sizeof (valsize)))
			return (error);
	} else
		valsize = 0;
	if ((error = sogetopt((struct socket *)fp->f_data, uap->level,
	    uap->name, &m)) == 0 && uap->val && valsize && m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), uap->val, (u_int)valsize);
		if (error == 0)
			error = copyout((caddr_t)&valsize,
			    (caddr_t)uap->avalsize, sizeof (valsize));
	}
	if (m != NULL)
		(void) m_free(m);
	return (error);
}

/* ARGSUSED */
pipe(p, uap, retval)
	struct proc *p;
	struct args *uap;
	int retval[];
{
	struct file *rf, *wf;
	struct socket *rso, *wso;
	int fd, error;

	if (error = socreate(AF_UNIX, &rso, SOCK_STREAM, 0))
		return (error);
	if (error = socreate(AF_UNIX, &wso, SOCK_STREAM, 0))
		goto free1;
	if (error = falloc(&rf, &fd))
		goto free2;
	retval[0] = fd;
	rf->f_flag = FREAD;
	rf->f_type = DTYPE_SOCKET;
	rf->f_ops = &socketops;
	rf->f_data = (caddr_t)rso;
	if (error = falloc(&wf, &fd))
		goto free3;
	wf->f_flag = FWRITE;
	wf->f_type = DTYPE_SOCKET;
	wf->f_ops = &socketops;
	wf->f_data = (caddr_t)wso;
	retval[1] = fd;
	if (error = unp_connect2(wso, rso))
		goto free4;
	return (0);
free4:
	wf->f_count = 0;
	u.u_ofile[retval[1]] = 0;
free3:
	rf->f_count = 0;
	u.u_ofile[retval[0]] = 0;
free2:
	(void)soclose(wso);
free1:
	(void)soclose(rso);
	return (error);
}

/*
 * Get socket name.
 */
#ifdef COMPAT_43
getsockname(p, uap, retval)
	struct proc *p;
	struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 0;
	return (getsockname1(p, uap, retval));
}

ogetsockname(p, uap, retval)
	struct proc *p;
	struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 1;
	return (getsockname1(p, uap, retval));
}
#else /* COMPAT_43 */

#define	getsockname1	getsockname
#endif

/* ARGSUSED */
getsockname1(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
#ifdef COMPAT_43
		int	compat_43;
#endif
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	int len, error;

	fp = getsock(uap->fdes, &error);
	if (fp == 0)
		return (error);
	if (error = copyin((caddr_t)uap->alen, (caddr_t)&len, sizeof (len)))
		return (error);
	so = (struct socket *)fp->f_data;
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	if (error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0))
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
#ifdef COMPAT_43
	if (uap->compat_43)
		mtod(m, struct osockaddr *)->sa_family =
		    mtod(m, struct sockaddr *)->sa_family;
#endif
	error = copyout(mtod(m, caddr_t), (caddr_t)uap->asa, (u_int)len);
	if (error == 0)
		error = copyout((caddr_t)&len, (caddr_t)uap->alen,
		    sizeof (len));
bad:
	m_freem(m);
	return (error);
}

/*
 * Get name of peer for connected socket.
 */
#ifdef COMPAT_43
getpeername(p, uap, retval)
	struct proc *p;
	struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 0;
	return (getpeername1(p, uap, retval));
}

ogetpeername(p, uap, retval)
	struct proc *p;
	struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
		int	compat_43;
	} *uap;
	int *retval;
{

	uap->compat_43 = 1;
	return (getpeername1(p, uap, retval));
}
#else /* COMPAT_43 */

#define	getpeername1	getpeername
#endif

/* ARGSUSED */
getpeername1(p, uap, retval)
	struct proc *p;
	register struct args {
		int	fdes;
		caddr_t	asa;
		int	*alen;
#ifdef COMPAT_43
		int	compat_43;
#endif
	} *uap;
	int *retval;
{
	register struct file *fp;
	register struct socket *so;
	struct mbuf *m;
	int len, error;

	fp = getsock(uap->fdes, &error);
	if (fp == 0)
		return (error);
	so = (struct socket *)fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
		return (ENOTCONN);
	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return (ENOBUFS);
	if (error = copyin((caddr_t)uap->alen, (caddr_t)&len, sizeof (len)))
		return (error);
	if (error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0))
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
#ifdef COMPAT_43
	if (uap->compat_43)
		mtod(m, struct osockaddr *)->sa_family =
		    mtod(m, struct sockaddr *)->sa_family;
#endif
	if (error = copyout(mtod(m, caddr_t), (caddr_t)uap->asa, (u_int)len))
		goto bad;
	error = copyout((caddr_t)&len, (caddr_t)uap->alen, sizeof (len));
bad:
	m_freem(m);
	return (error);
}

sockargs(mp, buf, buflen, type)
	struct mbuf **mp;
	caddr_t buf;
	int buflen, type;
{
	register struct mbuf *m;
	int error;

	if ((u_int)buflen > MLEN) {
#ifdef COMPAT_43
		if (type == MT_SONAME && (u_int)buflen <= 112)
			buflen = MLEN;		/* unix domain compat. hack */
		else
#endif
		return (EINVAL);
	}
	m = m_get(M_WAIT, type);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = buflen;
	error = copyin(buf, mtod(m, caddr_t), (u_int)buflen);
	if (error)
		(void) m_free(m);
	else
		*mp = m;
	if (type == MT_SONAME) {
		register struct sockaddr *sa = mtod(m, struct sockaddr *);

#if defined(COMPAT_43) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = buflen;
	}
	return (error);
}

struct file *
getsock(fdes, errp)
	int fdes, *errp;
{
	register struct file *fp;

	if ((unsigned)fdes >= NOFILE || (fp = u.u_ofile[fdes]) == NULL) {
		*errp = EBADF;
		return (0);
	}
	if (fp->f_type != DTYPE_SOCKET) {
		*errp = ENOTSOCK;
		return (0);
	}
	return (fp);
}
