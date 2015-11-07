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
#include "gtm_string.h"
#include "eintr_wrappers.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "iotcpdef.h"

GBLREF struct NTD *ntd_root;

error_def(ERR_GETNAMEINFO);
error_def(ERR_TEXT);

struct CLB *cmu_getclb(cmi_descriptor *node, cmi_descriptor *task)
{
	cmi_status_t status;
	struct CLB *p;
	que_ent_ptr_t qp;
	sigset_t oset;
	struct addrinfo *ai_ptr;
	int rc;

	status = cmj_getsockaddr(node, task, &ai_ptr);
	if (CMI_ERROR(status))
		return NULL;

	if (ntd_root)
	{
		SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
		for (qp = RELQUE2PTR(ntd_root->cqh.fl) ; qp != &ntd_root->cqh ;
				qp = RELQUE2PTR(p->cqe.fl))
		{
			p = QUEENT2CLB(qp, cqe);
			if (0 == memcpy(ai_ptr->ai_addr, (sockaddr_ptr)(&p->peer_sas), ai_ptr->ai_addrlen))
			{
				sigprocmask(SIG_SETMASK, &oset, NULL);
				return p;
			}
		}
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	}
	return NULL;
}
