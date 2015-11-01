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
#include "mlkdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlk_shr_init.h"

void mlk_shr_init(sm_uc_ptr_t base,
		  int4 size,
		  sgmnt_addrs *csa,
		  boolean_t read_write)
{
	int i, nr_blocks, nr_procs;
	sm_uc_ptr_t		cp;
	mlk_shrblk_ptr_t	sb;
	mlk_prcblk_ptr_t	pb;
	mlk_ctldata_ptr_t	ctl;

	nr_blocks = size / 64;
	nr_procs = size / 160;

	memset(base, 0, size);
	ctl = (mlk_ctldata_ptr_t)base;
	sb = (mlk_shrblk_ptr_t)(ctl + 1);
	ctl->wakeups = 1;
	A2R(ctl->blkfree, sb);
	ctl->blkcnt = nr_blocks;
	for (i = 1; i < nr_blocks ; i++, sb++)
	{
		A2R(sb->rsib, sb + 1);
	}
	pb = (mlk_prcblk_ptr_t)(sb + 1);
	A2R(ctl->prcfree, pb);
	ctl->prccnt = nr_procs;
	for (i = 1; i < nr_procs ; i++, pb++)
	{
		A2R(pb->next, pb + 1);
	}
	cp = (sm_uc_ptr_t)(pb + 1);
	A2R(ctl->subbase ,cp);
	A2R(ctl->subfree ,cp);
	cp = (sm_uc_ptr_t)base + size;
	A2R(ctl->subtop ,cp);
	assert(ctl->subtop > ctl->subbase);

	if (read_write)
		csa->hdr->trans_hist.lock_sequence = 0;

	return;
}
