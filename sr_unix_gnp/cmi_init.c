/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <errno.h>

#include "gtmio.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_ipv6.h"

#include "cmidef.h"
#include "eintr_wrappers.h"

GBLREF struct NTD *ntd_root;

error_def(CMI_BADPORT);
error_def(CMI_NOTND);
error_def(CMI_NETFAIL);

cmi_status_t cmi_init(cmi_descriptor *tnd, unsigned char tnr,
                void (*err)(struct NTD *, struct CLB *, cmi_reason_t reason),
		void (*crq)(struct CLB *),
		bool (*acc)(struct CLB *),
		void (*urg)(struct CLB *, unsigned char data),
		size_t pool_size,
		size_t usr_size,
		size_t mbl)
{
	cmi_status_t status = SS_NORMAL;
	char *envvar;
	struct protoent *p;
	unsigned short myport;
	sigset_t oset;
	int on = 1;
	int rval, rc, save_errno;
	struct addrinfo	*ai_ptr, *local_ai_ptr;
	boolean_t	af;

	status = cmj_netinit();
	if (CMI_ERROR(status))
		return status;

	status = cmj_getsockaddr(NULL, tnd, &local_ai_ptr);
	if (CMI_ERROR(status))
		return status;
	ntd_root->pool_size = pool_size;
	ntd_root->usr_size = usr_size;
	ntd_root->mbl = mbl;

	p = getprotobyname(GTCM_SERVER_PROTOCOL);
	endprotoent();
	if (!p)
		return CMI_NETFAIL;

	/* create the listening socket */
	if ((GTM_IPV6_SUPPORTED && !ipv4_only)
			&& (FD_INVALID != (ntd_root->listen_fd = socket(AF_INET6, SOCK_STREAM, p->p_proto))))
		af = AF_INET6;
	else if (FD_INVALID == (ntd_root->listen_fd = socket(AF_INET, SOCK_STREAM, p->p_proto)))
		return errno;
	else
		af = AF_INET;

	/* make sure we can re-run quickly w/o reuse problems */
	status = setsockopt(ntd_root->listen_fd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&on, SIZEOF(on));
	if (-1 == status)
	{
		save_errno = errno;
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);	/* resets "ntd_root->listen_fd" to FD_INVALID */
		return save_errno;
	}

	for(ai_ptr = local_ai_ptr; NULL != ai_ptr; ai_ptr = ai_ptr->ai_next)
		if (af == ai_ptr->ai_family)
			break;

	if (NULL == ai_ptr)
	{
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);
		return CMI_NETFAIL;
	}

	status = bind(ntd_root->listen_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
	freeaddrinfo(ai_ptr);

	if (-1 == status)
	{
		save_errno = errno;
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);	/* resets "ntd_root->listen_fd" to FD_INVALID */
		return save_errno;
	}

	status = cmj_setupfd(ntd_root->listen_fd);
	if (CMI_ERROR(status))
	{
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);	/* resets "ntd_root->listen_fd" to FD_INVALID */
		return status;
	}

	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	rval = listen(ntd_root->listen_fd, MAX_CONN_IND);
	if (-1 == rval)
	{
		save_errno = errno;
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);	/* resets "ntd_root->listen_fd" to FD_INVALID */
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		return save_errno;
	}
	status = cmj_set_async(ntd_root->listen_fd);
	if (CMI_ERROR(status))
	{
		CLOSEFILE_RESET(ntd_root->listen_fd, rc);	/* resets "ntd_root->listen_fd" to FD_INVALID */
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		return status;
	}
	FD_SET(ntd_root->listen_fd, &ntd_root->rs);
	FD_SET(ntd_root->listen_fd, &ntd_root->es);
	ntd_root->err = err;
	ntd_root->crq = crq;
	ntd_root->acc = acc;
	ntd_root->urg = urg;
	if (ntd_root->listen_fd > ntd_root->max_fd)
		ntd_root->max_fd = ntd_root->listen_fd;
	cmj_housekeeping(); /* will establish listening pools */
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	return SS_NORMAL;
}
