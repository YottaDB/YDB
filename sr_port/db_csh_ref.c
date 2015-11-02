/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gdsfhead.h"
#include "interlock.h"
#include "lockconst.h"
#include "longset.h"
#include "relqop.h"

error_def(ERR_WCFAIL);

GBLREF int4		process_id;

void	db_csh_ref(sgmnt_addrs *csa)
{
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		bp, bp_top;
	cache_rec_ptr_t		cr, cr_top, cr1;
	int4			buffer_size, rec_size;
	boolean_t		is_mm;

	csd = csa->hdr;
	/* Note the cr setups for MM should realistically be under a TARGETED_MSYNC_ONLY macro since the MM
	 * cache recs are only used in that mode. We don't currently use that mode but since this is one-time
	 * open-code, we aren't bothering. Note if targeted msyncs ever do come back into fashion, we should
	 * revisit the INTERLOCK_INIT_MM vs INTERLOCK_INIT usage, here and everywhere else too.
	 */
	is_mm = (dba_mm == csd->acc_meth);
	if (!is_mm)
	{
		longset((uchar_ptr_t)csa->acc_meth.bg.cache_state,
			SIZEOF(cache_que_heads) + (csd->bt_buckets + csd->n_bts - 1) * SIZEOF(cache_rec),
			0);						/* -1 since there is a cache_rec in cache_que_heads */
		cr = cr1 = csa->acc_meth.bg.cache_state->cache_array;
		buffer_size = csd->blk_size;
		assert(buffer_size > 0);
		assert(0 == buffer_size % DISK_BLOCK_SIZE);
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_wip.latch, LOCK_AVAILABLE);
		rec_size = SIZEOF(cache_rec);
	} else
	{
		longset((uchar_ptr_t)csa->acc_meth.mm.mmblk_state,
			SIZEOF(mmblk_que_heads) + (csd->bt_buckets + csd->n_bts - 1) * SIZEOF(mmblk_rec),
			0);						/* -1 since there is a mmblk_rec in mmblk_que_heads */
		cr = cr1 = (cache_rec_ptr_t)csa->acc_meth.mm.mmblk_state->mmblk_array;
		SET_LATCH_GLOBAL(&csa->acc_meth.mm.mmblk_state->mmblkq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&csa->acc_meth.mm.mmblk_state->mmblkq_wip.latch, LOCK_AVAILABLE);
		rec_size = SIZEOF(mmblk_rec);
	}
	cnl = csa->nl;
        SET_LATCH_GLOBAL(&cnl->wc_var_lock, LOCK_AVAILABLE);
        SET_LATCH_GLOBAL(&cnl->db_latch, LOCK_AVAILABLE);
	for (cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * csd->bt_buckets);
	     cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size))
		cr->blk = BT_QUEHEAD;
	cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * csd->n_bts);
	cnl->cur_lru_cache_rec_off = GDS_ANY_ABS2REL(csa, cr);
	cnl->cache_hits = 0;
	if (!is_mm)
	{
		bp = (sm_uc_ptr_t)ROUND_UP((sm_ulong_t)cr_top, OS_PAGE_SIZE);
		bp_top = bp + (gtm_uint64_t)csd->n_bts * buffer_size;
		GTMCRYPT_ONLY(
			if (csd->is_encrypted)
			{	/* In case of an encrypted database, bp_top is actually the beginning of the encrypted global buffer
				 * array (an array maintained parallely with the regular unencrypted global buffer array.
				 */
				cnl->encrypt_glo_buff_off = (sm_off_t)((sm_uc_ptr_t)bp_top - (sm_uc_ptr_t)bp);
			}
		)
	}
	for (;  cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size),
		     cr1 = (cache_rec_ptr_t)((sm_uc_ptr_t)cr1 + rec_size))
	{
		if (!is_mm)
		{
			INTERLOCK_INIT(cr);
		} else
		{
			INTERLOCK_INIT_MM(cr);
		}

		cr->cycle++;	/* increment cycle whenever buffer's blk number changes (for tp_hist) */
		cr->blk = CR_BLKEMPTY;
		assert(0 == cr->bt_index);	/* when cr->blk is empty, ensure no bt points to this cache-record */
		if (!is_mm)
		{
			assert(bp <= bp_top);
			cr->buffaddr = GDS_ANY_ABS2REL(csa, bp);
			bp += buffer_size;
		}
		insqt((que_ent_ptr_t)cr, (que_ent_ptr_t)cr1);
		cr->refer = FALSE;
	}
	cnl->wc_in_free = csd->n_bts;
	return;
}
