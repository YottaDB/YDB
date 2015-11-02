/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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

void	db_csh_ref(sgmnt_addrs *cs_addrs)
{
	sm_uc_ptr_t		bp, bp_top;
	cache_rec_ptr_t		cr, cr_top, cr1;
	int4			buffer_size, rec_size;
	bool			is_mm;

	is_mm = (dba_mm == cs_addrs->hdr->acc_meth);

	if (!is_mm)
	{
		longset((uchar_ptr_t)cs_addrs->acc_meth.bg.cache_state,
			sizeof(cache_que_heads) + (cs_addrs->hdr->bt_buckets + cs_addrs->hdr->n_bts - 1) * sizeof(cache_rec),
			0);						/* -1 since there is a cache_rec in cache_que_heads */
		cr = cr1 = cs_addrs->acc_meth.bg.cache_state->cache_array;
		buffer_size = cs_addrs->hdr->blk_size;
		assert(buffer_size > 0);
		assert(0 == buffer_size % DISK_BLOCK_SIZE);
		SET_LATCH_GLOBAL(&cs_addrs->acc_meth.bg.cache_state->cacheq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&cs_addrs->acc_meth.bg.cache_state->cacheq_wip.latch, LOCK_AVAILABLE);
		rec_size = sizeof(cache_rec);
	}
	else
	{
		longset((uchar_ptr_t)cs_addrs->acc_meth.mm.mmblk_state,
			sizeof(mmblk_que_heads) + (cs_addrs->hdr->bt_buckets + cs_addrs->hdr->n_bts - 1) * sizeof(mmblk_rec),
			0);						/* -1 since there is a mmblk_rec in mmblk_que_heads */
		cr = cr1 = (cache_rec_ptr_t)cs_addrs->acc_meth.mm.mmblk_state->mmblk_array;
		SET_LATCH_GLOBAL(&cs_addrs->acc_meth.mm.mmblk_state->mmblkq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&cs_addrs->acc_meth.mm.mmblk_state->mmblkq_wip.latch, LOCK_AVAILABLE);
		rec_size = sizeof(mmblk_rec);
	}

        SET_LATCH_GLOBAL(&cs_addrs->nl->wc_var_lock, LOCK_AVAILABLE);
        SET_LATCH_GLOBAL(&cs_addrs->nl->db_latch, LOCK_AVAILABLE);
	for (cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * cs_addrs->hdr->bt_buckets);
			cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size))
		cr->blk = BT_QUEHEAD;
	cr_top = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size * cs_addrs->hdr->n_bts);
	cs_addrs->nl->cur_lru_cache_rec_off = GDS_ABS2REL(cr);
	cs_addrs->nl->cache_hits = 0;

	if (!is_mm)
	{
		bp = (sm_uc_ptr_t)ROUND_UP((sm_ulong_t)cr_top, OS_PAGE_SIZE);
		bp_top = bp + cs_addrs->hdr->n_bts * buffer_size;
	}

	for (;  cr < cr_top;  cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + rec_size),
				cr1 = (cache_rec_ptr_t)((sm_uc_ptr_t)cr1 + rec_size))
	{
		if (is_mm)
			INTERLOCK_INIT_MM(cr);
		else
			INTERLOCK_INIT(cr);

		cr->cycle++;	/* increment cycle whenever buffer's blk number changes (for tp_hist) */
		cr->blk = CR_BLKEMPTY;
		assert(0 == cr->bt_index);	/* when cr->blk is empty, ensure no bt points to this cache-record */
		if  (!is_mm)
		{
			assert(bp <= bp_top);
			cr->buffaddr = GDS_ABS2REL(bp);
			bp += buffer_size;
		}
		insqt((que_ent_ptr_t)cr, (que_ent_ptr_t)cr1);
		cr->refer = FALSE;
	}
	cs_addrs->nl->wc_in_free = cs_addrs->hdr->n_bts;
	return;
}
