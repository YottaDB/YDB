/****************************************************************
 *                                                              *
 *      Copyright 2012 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
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
#include "t_recycled2free.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF	unsigned int	t_tries;

boolean_t t_recycled2free(srch_blk_status *blkhist)
{
	cw_set_element		*cse;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;
	sgmnt_addrs		*csa;
	jnl_buffer_ptr_t	jbbp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	assert(cw_set_depth < CDB_CW_SET_SIZE);
	assert(dba_bg == csa->hdr->acc_meth);
	cse = &cw_set[cw_set_depth];
	cse->mode = gds_t_recycled2free;
	cse->blk = blkhist->blk_num;
	cse->old_block = blkhist->buffaddr;
	old_block = (blk_hdr_ptr_t)cse->old_block;
	/* t_recycled2free operates on RECYCLED blocks and hence cse->blk_prior_state's free_status is set to FALSE */
	BIT_CLEAR_FREE(cse->blk_prior_state);
	BIT_SET_RECYCLED(cse->blk_prior_state); /* set recycled bit as this is relied upon to write to snapshot file */
	cse->level = old_block->levl; /* maintain level information so only level>0 will be written to snapshot file */
	cse->cr = blkhist->cr;
	cse->cycle = blkhist->cycle;
	cse->blk_checksum = 0;
	assert(NULL != old_block);
	jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
	{	/* Pre-compute CHECKSUM */
		bsiz = old_block->bsiz;
		if (bsiz > cs_data->blk_size)
		{
			assert(CDB_STAGNATE > t_tries);
			return FALSE; /* restart */
		}
		JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, cs_data, cs_addrs, cse->old_block, bsiz);
	}
	cse->upd_addr = NULL;
	cse->jnl_freeaddr = 0;      /* reset jnl_freeaddr that previous transaction might have filled in */
	cse->done = FALSE;
	cse->new_buff = NULL;
	blkhist->cse = cse;
	cw_set_depth++;
	return TRUE;
}

