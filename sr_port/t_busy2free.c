/****************************************************************
 *								*
 *	Copyright 2007, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "t_busy2free.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF uint4		dollar_tlevel;
GBLREF sgmnt_addrs	*cs_addrs;

void	t_busy2free(srch_blk_status *blkhist)
{
	cw_set_element		*cse;
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;
	jnl_buffer_ptr_t	jbbp;		/* jbbp is non-NULL only if before-image journaling */

	assert(!dollar_tlevel);
	assert(cw_set_depth < CDB_CW_SET_SIZE);
	cse = &cw_set[cw_set_depth];
	cse->mode = gds_t_busy2free;
	cse->blk = blkhist->blk_num;
	cse->old_block = blkhist->buffaddr;
	old_block = (blk_hdr_ptr_t)cse->old_block;
	/* t_busy2free operates on BUSY blocks and hence cse->blk_prior_state's free and recycled status is always set to FALSE */
	BIT_CLEAR_FREE(cse->blk_prior_state);
	BIT_CLEAR_RECYCLED(cse->blk_prior_state);
	cse->blk_checksum = 0;
	csa = cs_addrs;
	assert(dba_bg == csa->hdr->acc_meth);
	assert(NULL != old_block);
	jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
	{	/* Pre-compute CHECKSUM. Since we dont necessarily hold crit at this point, ensure we never try to
		 * access the buffer more than the db blk_size.
		 */
		bsiz = MIN(old_block->bsiz, csa->hdr->blk_size);
		cse->blk_checksum = jnl_get_checksum((uint4*)old_block, csa, bsiz);
	}
	cse->upd_addr = NULL;
	cse->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cse->done = FALSE;
	blkhist->cse = cse;	/* indicate to t_end/tp_tend that this block is part of the write-set */
	cw_set_depth++;
}
