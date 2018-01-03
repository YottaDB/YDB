/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>

#include "gtm_string.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "jnl_write.h"
#include "jnl_write_pblk.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		dse_running;

void	jnl_write_pblk(sgmnt_addrs *csa, cw_set_element *cse, uint4 com_csum)
{
	blk_hdr_ptr_t		old_block;
	int			tmp_jrec_size;
	jnl_buffer_ptr_t	jbp;
	jnl_private_control	*jpc;
	sgmnt_data_ptr_t	csd;
	struct_jrec_blk		pblk_record;
	unsigned int		bsiz;
#	ifdef DEBUG
	blk_hdr_ptr_t		save_old_block;
#	endif

	csd = csa->hdr;
	assert(IN_PHASE2_JNL_COMMIT(csa));
	old_block = (blk_hdr_ptr_t)cse->old_block;
	ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
	/* Assert that cr corresponding to old_block is pinned in shared memory */
	DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, (dba_mm == csd->acc_meth), csa, csd);
	bsiz = old_block->bsiz;
	bsiz = MIN(bsiz, csd->blk_size);	/* be safe in PRO */
	if (!cse->blk_checksum)
		cse->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
	else
		assert(cse->blk_checksum == jnl_get_checksum(old_block, csa, bsiz));
	if (NEEDS_ANY_KEY(csd, old_block->tn))
	{
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)old_block);
		DEBUG_ONLY(save_old_block = old_block;)
		old_block = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(old_block, csa);
		/* Ensure that unencrypted block and its twin counterpart are in sync. */
		assert(save_old_block->tn == old_block->tn);
		assert(save_old_block->bsiz == old_block->bsiz);
		assert(save_old_block->levl == old_block->levl);
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)old_block);
	}
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr) || ((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa))));
	pblk_record.prefix.jrec_type = JRT_PBLK;
	pblk_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	pblk_record.prefix.tn = JB_CURR_TN_APPROPRIATE(TRUE, jpc, csa);
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	pblk_record.prefix.time = jgbl.gbl_jrec_time;
	pblk_record.blknum = cse->blk;
	/* in case we have a bad block-size, we don't want to write a PBLK larger than the GDS block size (maximum block size).
	 * in addition, check that checksum computed in t_end/tp_tend did take the adjusted bsiz into consideration.
	 */
	assert(old_block->bsiz <= csd->blk_size || dse_running);
	pblk_record.bsiz = bsiz;
	assert((pblk_record.bsiz == old_block->bsiz) || (cse->blk_checksum == jnl_get_checksum(old_block, NULL, pblk_record.bsiz)));
	assert(pblk_record.bsiz >= SIZEOF(blk_hdr) || dse_running);
	pblk_record.ondsk_blkver = cse->ondsk_blkver;
	tmp_jrec_size = (int)FIXED_PBLK_RECLEN + pblk_record.bsiz + JREC_SUFFIX_SIZE;
	pblk_record.prefix.forwptr = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	COMPUTE_PBLK_CHECKSUM(cse->blk_checksum, &pblk_record, com_csum, pblk_record.prefix.checksum);
	assert(SIZEOF(uint4) == SIZEOF(jrec_suffix));
	jnl_write(jpc, JRT_PBLK, (jnl_record *)&pblk_record, old_block);
	jbp = jpc->jnl_buff;
	cse->jnl_freeaddr = JB_FREEADDR_APPROPRIATE(TRUE, jpc, jbp);
}
