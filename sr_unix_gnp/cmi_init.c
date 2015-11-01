/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "cmidef.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include <netinet/in.h>
#include <errno.h>

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
	struct sockaddr_in in;
	sigset_t oset;
	int on = 1;
	int rval, rc, save_errno;

	status = cmj_netinit();
	if (CMI_ERROR(status))
		return status;

	status = cmj_getsockaddr(tnd, &in);
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
	ntd_root->listen_fd = socket(AF_INET, SOCK_STREAM, p->p_proto);
	if (-1 == ntd_root->listen_fd)
		return errno;

	/* make sure we can re-run quickly w/o reuse problems */
	status = setsockopt(ntd_root->listen_fd, SOL_SOCKET, SO_REUSEADDR,
		(void*)&on, sizeof(on));
	if (-1 == status)
	{
		save_errno = errno;
		CLOSEFILE(ntd_root->listen_fd, rc);
		ntd_root->listen_fd = -1;
		return save_errno;
	}

	status = bind(ntd_root->listen_fd, (struct sockaddr*)&in, sizeof(in));
	if (-1 == status)
	{
		save_errno = errno;
		CLOSEFILE(ntd_root->listen_fd, rc);
		ntd_root->listen_fd = -1;
		return save_errno;
	}

	status = cmj_setupfd(ntd_root->listen_fd);
	if (CMI_ERROR(status))
	{
		CLOSEFILE(ntd_root->listen_fd, rc);
		ntd_root->listen_fd = -1;
		return status;
	}

	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	rval = listen(ntd_root->listen_fd, MAX_CONN_IND);
	if (-1 == rval)
	{
		save_errno = errno;
		CLOSEFILE(ntd_root->listen_fd, rc);
		ntd_root->listen_fd = -1;
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
		return save_errno;
	}
	status = cmj_set_async(ntd_root->listen_fd);
	if (CMI_ERROR(status))
	{
		CLOSEFILE(ntd_root->listen_fd, rc);
		ntd_root->listen_fd = -1;
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
