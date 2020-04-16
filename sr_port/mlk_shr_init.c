/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 * 								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

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
	int			i, nr_blocks, nr_procs, shr_size, nr_buckets;
	float			shrblk_adj, shrhash_adj;
	sm_uc_ptr_t		cp;
	mlk_shrhash_ptr_t	sh;
	mlk_shrblk_ptr_t	sb;
	mlk_prcblk_ptr_t	pb;
	mlk_ctldata_ptr_t	ctl;

	/* there are four sections with the following approximate sizes
	 *  mlk_ctldata	--> SIZEOF(mlk_ctldata)
	 *  mlk_shrblk	--> size * 5/8 (consisting of nr_blocks number of mlk_shrblk structures)
	 *  mlk_prcblk	--> size * 1/8 (consisting of nr_procs  number of mlk_prcblk structures)
	 *  mlk_shrsubs	--> size * 2/8 - SIZEOF(mlk_ctldata) (consisting of variable number of variable length subscript strings)
	 * Total block counts are recorded in ctl->max_* because those values are used for free lock space calculation.
	 * (see:lke_show.c and mlk_unlock.c)
	 */

	shr_size = (size >> 1) + (size >> 3);	/* size/2 + size/8 = size*5/8 */
	/* Split shrblock space into blocks and hash buckets.
	 * Hopscotch hashing works reasonably well with a high load factor, but leaving a little space avoids the worst case,
	 * and more buckets helps avoid excess collisions, so allocate up to 1/4 more hash buckets than shrblks.
	 *
	 * Note shrblk_adj can be increased in small fractional steps to compensate for an increase in mlk_shrblk size that
	 * results in a disproportionate decrease in the number of mlk_shrblks relative to the other block types.
	 *
	 */
	shrblk_adj = 1.1;
	shrhash_adj = 1 + MLK_HASH_EXCESS;
	nr_blocks = (shrblk_adj * shr_size) / (shrblk_adj * SIZEOF(mlk_shrblk) + shrhash_adj * SIZEOF(mlk_shrhash));
	nr_buckets = (shrhash_adj * shr_size) / (shrblk_adj * SIZEOF(mlk_shrblk) + shrhash_adj * SIZEOF(mlk_shrhash));
	assert(shr_size >= (nr_blocks * SIZEOF(mlk_shrblk) + nr_buckets * SIZEOF(mlk_shrhash)));
	nr_procs = (size >> 3) / SIZEOF(mlk_prcblk);
	memset(base, 0, size);
	ctl = (mlk_ctldata_ptr_t)base;
	ctl->wakeups = 1;
	sh = (mlk_shrhash_ptr_t)(ctl + 1);
	A2R(ctl->blkhash, sh);
	ctl->num_blkhash = nr_buckets;
	sb = (mlk_shrblk_ptr_t)(sh + nr_buckets);
	A2R(ctl->blkbase, sb);
	A2R(ctl->blkfree, sb);
	ctl->blkcnt = nr_blocks;
	ctl->max_blkcnt = nr_blocks;
	for (i = 1; i < nr_blocks ; i++, sb++)
		A2R(sb->rsib, sb + 1);
	pb = (mlk_prcblk_ptr_t)(sb + 1);
	A2R(ctl->prcfree, pb);
	ctl->prccnt = nr_procs;
	ctl->max_prccnt = nr_procs;
	for (i = 1; i < nr_procs ; i++, pb++)
	{
		A2R(pb->next, pb + 1);
	}
	cp = (sm_uc_ptr_t)(pb + 1);
	A2R(ctl->subbase, cp);
	A2R(ctl->subfree, cp);
	cp = (sm_uc_ptr_t)base + size;
	A2R(ctl->subtop ,cp);
	assert(ctl->subtop > ctl->subbase);
	if (read_write)
		csa->hdr->trans_hist.lock_sequence = 0;
	ctl->hash_shmid = INVALID_SHMID;
	ctl->hash_seed = 0;
	return;
}
