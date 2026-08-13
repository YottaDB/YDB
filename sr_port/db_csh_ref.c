/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "interlock.h"
#include "lockconst.h"
#include "longset.h"
#include "relqop.h"
#include "filestruct.h"
#include "jnl.h"

error_def(ERR_WCFAIL);

GBLREF uint4		process_id;
#ifdef DEBUG
GBLREF jnl_gbls_t	jgbl;
#endif

/* Refresh the database cache records in the shared memory. If init = TRUE, do the longset and initialize all the latches and
 * forward and backward links. If init = FALSE, just increment the cycle and set the blk to CR_BLKEMPTY.
 */
void	db_csh_ref(sgmnt_addrs *csa, boolean_t init)
{
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		bp, bp_top, sidx;
	cache_rec_ptr_t		cr, cr_top, cr1;
	int4			buffer_size, rec_size;

	csd = csa->hdr;
	cnl = csa->nl;
	if (init)
	{
		SET_LATCH_GLOBAL(&cnl->wc_var_lock, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&cnl->db_latch, LOCK_AVAILABLE);
		if (dba_mm == csd->acc_meth)
		{	/* YDB#1143 : MM lays its search index array out at the very end of the shared memory
			 * segment, after the file header, because an MM segment has no global buffer array for
			 * it to follow - see the BG case below, and "dbsecspc" for the matching sizing. Setting
			 * the offset is what turns the feature on for every process attached to this shared
			 * memory segment; leaving it zero turns it off. Done HERE and not where this function is
			 * called from, so that both access methods settle "search_idx_off" in one file.
			 */
			if ((0 != csd->search_idx_size) && (0 < csd->search_idx_slots))
			{
				assert(0 == (csd->search_idx_size % 8));
				cnl->search_idx_off = GDS_ANY_ABS2REL(csa,
						(sm_uc_ptr_t)csd + ROUND_UP(SIZEOF_FILE_HDR(csd), OS_PAGE_SIZE));
			}
			return;
		}
		longset((uchar_ptr_t)csa->acc_meth.bg.cache_state,
			SIZEOF(cache_que_heads) + (csd->bt_buckets + csd->n_bts - 1) * SIZEOF(cache_rec),
			0);					/* -1 since there is a cache_rec in cache_que_heads */
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_wip.latch, LOCK_AVAILABLE);
	}
	/* Everything from here on is BG. The "init" caller returned above for MM, and the only caller that
	 * passes init FALSE is "mur_process_intrpt_recov" for an ONLINE rollback, which an MM region cannot
	 * have : online rollback needs BEFORE image journaling and MM does not support it. The line below
	 * has always depended on that - "acc_meth.bg" is a union member that holds nothing under MM - so
	 * assert it rather than leave it as an unstated assumption.
	 */
	assert(dba_bg == csd->acc_meth);
	cr = cr1 = csa->acc_meth.bg.cache_state->cache_array;
	buffer_size = csd->blk_size;
	assert(buffer_size > 0);
	assert(0 == buffer_size % DISK_BLOCK_SIZE);
	rec_size = SIZEOF(cache_rec);
	for (cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * csd->bt_buckets);
	     cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size))
		cr->blk = BT_QUEHEAD;
	cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * csd->n_bts);
	if (init)
	{
		cnl->cur_lru_cache_rec_off = GDS_ANY_ABS2REL(csa, cr);
		cnl->cache_hits = 0;
	}
	bp = (sm_uc_ptr_t)ROUND_UP((sm_ulong_t)cr_top, OS_PAGE_SIZE);
	bp_top = bp + (gtm_uint64_t)csd->n_bts * buffer_size;
	/* In case of an encrypted database, bp_top is actually the beginning of the encrypted global buffer array (an array
	 * maintained parallely with the regular unencrypted global buffer array.
	 */
	if (USES_ENCRYPTION(csd->is_encrypted))
		cnl->encrypt_glo_buff_off = (sm_off_t)((sm_uc_ptr_t)bp_top - (sm_uc_ptr_t)bp);
	/* YDB#1143 : the search index array follows whichever buffer array is last. Setting the offset is
	 * what turns the feature on for every process attached to this shared memory segment; leaving it
	 * zero turns it off.
	 */
	if ((0 != csd->search_idx_size) && (0 < csd->search_idx_slots))
	{	/* BG, per the assert above. MM lays its own array out in the "init" branch, since an MM
		 * segment has no global buffer array for it to follow.
		 */
		sidx = bp_top;
		if (USES_ENCRYPTION(csd->is_encrypted))
			sidx += (gtm_uint64_t)csd->n_bts * buffer_size;
		assert(0 == (csd->search_idx_size % 8));
		if (init)
			cnl->search_idx_off = GDS_ANY_ABS2REL(csa, sidx);
		else
		{	/* Online rollback, reached from "mur_process_intrpt_recov". It works with the shared
			 * memory layout it found - other processes stay attached and have to keep operating -
			 * so the buffer arrays cannot move and this offset cannot change. Assert that instead
			 * of storing it again, because every process derived its private search_idx_base from
			 * this value once in "db_init" and never refreshes it: were the offset to move here,
			 * those copies would point into the wrong place with nothing to notice.
			 */
			assert(cnl->search_idx_off == GDS_ANY_ABS2REL(csa, sidx));
		}
	}
	for (;  cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size),
		     cr1 = (cache_rec_ptr_t)((sm_uc_ptr_t)cr1 + rec_size))
	{
		if (init)
			INTERLOCK_INIT(cr);
		cr->cycle++;	/* increment cycle whenever buffer's blk number changes (for tp_hist) */
		cr->blk = CR_BLKEMPTY;
		/* Typically db_csh_ref is invoked from db_init (when creating shared memory afresh) in which case cr->bt_index
		 * will be 0 (due to the memset). But, if db_csh_ref is invoked from online rollback where we are invalidating the
		 * cache but do not do the longset (above), bt_index can be a non-zero value. Assert that and set it to zero since
		 * we are invalidating the cache record anyways.
		 */
		assert((0 == cr->bt_index) || jgbl.onlnrlbk);
		cr->bt_index = 0;
		if (init)
		{
			assert(bp <= bp_top);
			cr->buffaddr = GDS_ANY_ABS2REL(csa, bp);
			bp += buffer_size;
			insqt((que_ent_ptr_t)cr, (que_ent_ptr_t)cr1);
		}
		cr->refer = FALSE;
	}
	cnl->wc_in_free = csd->n_bts;
	return;
}
