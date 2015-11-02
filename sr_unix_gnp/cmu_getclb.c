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
#include "cmidef.h"
#include "gtm_string.h"
#include "eintr_wrappers.h"

GBLREF struct NTD *ntd_root;

struct CLB *cmu_getclb(cmi_descriptor *node, cmi_descriptor *task)
{
	cmi_status_t status;
	struct CLB *p;
	que_ent_ptr_t qp;
	sigset_t oset;
	struct sockaddr_in in;
	int rc;

	status = cmj_resolve_nod_tnd(node, task, &in);
	if (CMI_ERROR(status))
		return NULL;

	if (ntd_root)
	{
		SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
		for (qp = RELQUE2PTR(ntd_root->cqh.fl) ; qp != &ntd_root->cqh ;
				qp = RELQUE2PTR(p->cqe.fl))
		{
			p = QUEENT2CLB(qp, cqe);
			if (p->peer.sin_port == in.sin_port && 0 == memcmp(&p->peer.sin_addr, &in.sin_addr, SIZEOF(in.sin_addr)))
			{ /* (port, address) pair is necessary and sufficient for uniqueness. There might be other fields in
			     sockaddr_in that might be implementation dependent. So, compare only (port, address) pair. */
				sigprocmask(SIG_SETMASK, &oset, NULL);
				return p;
			}
		}
		SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	}
	return NULL;
}
