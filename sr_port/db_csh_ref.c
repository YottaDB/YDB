/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

GBLREF int4		process_id;
#if defined(UNIX) && defined(DEBUG)
GBLREF jnl_gbls_t	jgbl;
#endif
/* Refresh the database cache records in the shared memory. If init = TRUE, do the longset and initialize all the latches and
 * forward and backward links. If init = FALSE, just increment the cycle and set the blk to CR_BLKEMPTY.
 */
void	db_csh_ref(sgmnt_addrs *csa, boolean_t init)
{
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		bp, bp_top;
	cache_rec_ptr_t		cr, cr_top, cr1;
	int4			buffer_size, rec_size;

	csd = csa->hdr;
	cnl = csa->nl;
	if (init)
	{
		SET_LATCH_GLOBAL(&cnl->wc_var_lock, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&cnl->db_latch, LOCK_AVAILABLE);
		if (dba_mm == csd->acc_meth)
			return;
		longset((uchar_ptr_t)csa->acc_meth.bg.cache_state,
			SIZEOF(cache_que_heads) + (csd->bt_buckets + csd->n_bts - 1) * SIZEOF(cache_rec),
			0);					/* -1 since there is a cache_rec in cache_que_heads */
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_active.latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&csa->acc_meth.bg.cache_state->cacheq_wip.latch, LOCK_AVAILABLE);
	}
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
	GTMCRYPT_ONLY(
		if (csd->is_encrypted)
		{	/* In case of an encrypted database, bp_top is actually the beginning of the encrypted global buffer
			 * array (an array maintained parallely with the regular unencrypted global buffer array.
			 */
			cnl->encrypt_glo_buff_off = (sm_off_t)((sm_uc_ptr_t)bp_top - (sm_uc_ptr_t)bp);
		}
	)
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
		assert((0 == cr->bt_index) UNIX_ONLY(|| jgbl.onlnrlbk));
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
