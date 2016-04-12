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
#include "relqop.h"
#include "eintr_wrappers.h"

GBLREF struct NTD *ntd_root;

void cmi_free_clb(struct CLB *lnk)
{
	sigset_t oset;
	struct NTD *tsk;
	int rc;

	if (!lnk)
		return;
	tsk = lnk->ntd;
	insqt(&lnk->cqe, &tsk->cqh_free);
	ntd_root->freelist_dirty = TRUE;
	ntd_root->free_count++;
	SIGPROCMASK(SIG_BLOCK, &tsk->mutex_set, &oset, rc);
	cmj_housekeeping();
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
}

struct CLB *cmi_alloc_clb(void)
{
	struct CLB *lnk;
	que_ent_ptr_t qp;

	if (ntd_root && ntd_root->pool_size)
	{
		/* server path */
		qp = remqh(&ntd_root->cqh_free);
		if (qp)
		{
			lnk = QUEENT2CLB(qp, cqe);
			ntd_root->free_count--;
		}
	}
	else
	{
		/* client path */
		lnk = (struct CLB *)malloc(SIZEOF(*lnk));
		cmj_init_clb(NULL, lnk);
	}
	return lnk;
}

unsigned char *cmi_realloc_mbf(struct CLB *lnk, size_t new_size)
{

	if (lnk->mbf)
	{
		lnk->mbl = 0;
		free(lnk->mbf);
	}
	lnk->mbf = (unsigned char *)malloc(new_size);
	if (lnk->mbf)
		lnk->mbl = new_size;
	return lnk->mbf;
}
