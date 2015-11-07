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
#include "cmidef.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#ifdef __sparc
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include "gtm_inet.h"
#include <errno.h>
#include "gtmio.h"
#include "relqop.h"
#include "gtm_string.h" /* for memcpy */

void cmj_incoming_call(struct NTD *tsk)
{
	int rval, rc;
	struct CLB *lnk;
	struct sockaddr_storage sas;
	GTM_SOCKLEN_TYPE sz = SIZEOF(struct sockaddr);
	cmi_status_t status;

	while ((-1 == (rval = ACCEPT(tsk->listen_fd, (struct sockaddr *)&sas, (GTM_SOCKLEN_TYPE *)&sz))) && EINTR == errno)
		;
	while (rval >= 0)
	{
		status = cmj_setupfd(rval);
		if (CMI_ERROR(status))
		{
			CLOSEFILE_RESET(rval, rc);	/* resets "rval" to FD_INVALID */
			return;
		}
		status = cmj_set_async(rval);
		if (CMI_ERROR(status))
		{
			CLOSEFILE_RESET(rval, rc);	/* resets "rval" to FD_INVALID */
			return;
		}

		/* grab a clb off of the free list */
		lnk = cmi_alloc_clb();
		if (!lnk || !tsk->acc || !tsk->acc(lnk) || !tsk->crq)
		{
			/* no point if the callbacks are not in place */
			cmi_free_clb(lnk);
			CLOSEFILE_RESET(rval, rc);	/* resets "rval" to FD_INVALID */
			return;
		}
		if (rval > tsk->max_fd)
			tsk->max_fd = rval;
		lnk->mun = rval;
		lnk->sta = CM_CLB_IDLE;
		memcpy(&lnk->peer_sas, &sas, sz);
		lnk->peer_ai.ai_addr = (struct sockaddr *)&lnk->peer_sas;
		lnk->peer_ai.ai_addrlen = sz;
		insqh(&lnk->cqe, &tsk->cqh);
		lnk->ntd = tsk;
		FD_SET(rval, &tsk->es);
		/* setup for callback processing */
		lnk->deferred_event = TRUE;
		lnk->deferred_reason = CMI_REASON_CONNECT;
		while ((-1 == (rval = ACCEPT(tsk->listen_fd, (struct sockaddr *)&sas, (GTM_SOCKLEN_TYPE *)&sz)))
			 && EINTR == errno);
	}
}
