/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "have_crit.h"

GBLREF struct NTD *ntd_root;

void cmj_housekeeping(void)
{
	struct CLB *lnk;
	que_ent_ptr_t qp;
	sigset_t oset;

	ASSERT_IS_LIBCMISOCKETTCP;
	/* Check if any process terminating signal was received (e.g. SIGTERM) and if so handle it now
	 * when it is safe to do so (i.e. while we are not inside a signal handler).
	 */
	DEFERRED_SIGNAL_HANDLING_CHECK;
	/* handle I/O interrupts */
	if (ntd_root->sigurg_interrupt)
	{
		cmj_select(SIGURG);
		ntd_root->sigurg_interrupt = FALSE;
	}
	if (ntd_root->sigio_interrupt)
	{
		cmj_select(SIGIO);
		ntd_root->sigio_interrupt = FALSE;
	}
	/* prune CLB free list if necessary */
	while (ntd_root->free_count > ntd_root->pool_size)
	{
		qp = remqh(&ntd_root->cqh_free);
		if (qp)
		{
			lnk = QUEENT2CLB(qp, cqe);
			assert(lnk);
			if (ntd_root->mbl && lnk->mbf)
				free(lnk->mbf);
			free(lnk);
			ntd_root->free_count--;
		}
	}
	/* coerce mbf back to mbl sized buffers */
	if (ntd_root->freelist_dirty)
	{
		ntd_root->freelist_dirty = FALSE;
		for (qp = RELQUE2PTR(ntd_root->cqh_free.fl) ; ntd_root->mbl && qp != &ntd_root->cqh_free ;
				qp = RELQUE2PTR(lnk->cqe.fl))
		{
			lnk = QUEENT2CLB(qp, cqe);
			if (lnk->mbl != ntd_root->mbl)
				(void)cmi_realloc_mbf(lnk, ntd_root->mbl);
		}
	}
	/* add to the free list up to poll size if necessary */
	while (ntd_root->free_count < ntd_root->pool_size)
	{
		lnk = (struct CLB *)malloc(SIZEOF(*lnk) + ntd_root->usr_size);
		assert(lnk);
		cmj_init_clb(ntd_root, lnk);
		if (ntd_root->mbl)
		{
			lnk->mbf = (unsigned char *)malloc(ntd_root->mbl);
			if (lnk->mbf)
				lnk->mbl = ntd_root->mbl;
		}
		insqt(&lnk->cqe, &ntd_root->cqh_free);
		ntd_root->free_count++;
	}
}
