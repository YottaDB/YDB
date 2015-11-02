/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include "gtm_string.h"

#include "cmidef.h"
#include "gtmio.h"
#include "eintr_wrappers.h"

#include "relqop.h"

GBLREF struct NTD *ntd_root;

GBLREF	volatile int4	outofband;

error_def(CMI_NETFAIL);

cmi_status_t cmi_open(struct CLB *lnk)
{
	cmi_status_t status;
	int rval;
	unsigned char *cp;
	char hn[MAX_HOST_NAME_LEN];
	struct sockaddr_in in;
	struct hostent *hp;
	struct protoent *p;
	sigset_t oset;
	int new_fd, rc, save_errno;

	if (!ntd_root)
	{
		status = cmj_netinit();
		if (CMI_ERROR(status))
			return status;
	}
	lnk->ntd = ntd_root;

	status = cmj_resolve_nod_tnd(&lnk->nod, &lnk->tnd, &in);
	if (CMI_ERROR(status))
		return status;

	p = getprotobyname(GTCM_SERVER_PROTOCOL);
	endprotoent();
	if (!p)
		return CMI_NETFAIL;

	lnk->mun = -1;
	memset((void *)&lnk->cqe, 0, sizeof(lnk->cqe));
	memset((void *)&lnk->ios, 0, sizeof(lnk->ios));
	memset((void *)&lnk->stt, 0, sizeof(lnk->stt));
	lnk->cbl = 0;
	lnk->urgdata = 0;
	lnk->deferred_event = lnk->deferred_reason = lnk->deferred_status = 0;
	lnk->sta = CM_CLB_DISCONNECT;
	lnk->err = NULL;
	lnk->ast = NULL;
	new_fd = socket(AF_INET, SOCK_STREAM, p->p_proto);
	if (FD_INVALID == new_fd)
		return errno;

	while ((-1 == (rval = connect(new_fd, (struct sockaddr *)&in, sizeof(in)))) && EINTR ==errno && 0 == outofband)
		;
	if (-1 == rval)
	{
		save_errno = errno;
		CLOSEFILE_RESET(new_fd, rc);	/* resets "new_fd" to FD_INVALID */
		return save_errno;
	}
	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	status = cmj_setupfd(new_fd);
	if (CMI_ERROR(status))
		CLOSEFILE_RESET(new_fd, rc)	/* resets "new_fd" to FD_INVALID */
	else
	{
		status = cmj_set_async(new_fd);
		if (!CMI_ERROR(status))
		{
			insqh(&lnk->cqe, &ntd_root->cqh);
			if (new_fd > ntd_root->max_fd)
				ntd_root->max_fd = new_fd;
			lnk->mun = new_fd;
			lnk->peer = in;
			FD_SET(new_fd, &ntd_root->es);
			lnk->sta = CM_CLB_IDLE;
		} else
			CLOSEFILE_RESET(new_fd, rc);	/* resets "new_fd" to FD_INVALID */
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	return status;
}
