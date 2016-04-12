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
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "cache.h"
#include "hashtab.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for cws_insert.h */
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF unsigned int	t_tries;
GBLREF boolean_t        mu_reorg_process;

#define ENOUGH_TRIES_TO_FALL_BACK 17

/* This function returns a pointer to the cache_rec entry, NULL if not found, or CR_INVALID if the hash table is corrupt */

cache_rec_ptr_t	db_csh_get(block_id block) /* block number to look up */
{
	register sgmnt_addrs		*csa;
	sgmnt_data_ptr_t		csd;
	cache_rec_ptr_t			cr, cr_hash_base;
	int				blk_hash, lcnt, ocnt, hmax;
#	ifdef DEBUG
	cache_rec_ptr_t			cr_low, cr_high;
#	endif

	csa = cs_addrs;
	csd = csa->hdr;
	assert(dba_mm != csd->acc_meth);
	hmax = csd->bt_buckets;
	blk_hash = (block % hmax);
	DEBUG_ONLY(cr_low = &csa->acc_meth.bg.cache_state->cache_array[0];)
	DEBUG_ONLY(cr_high = cr_low + csd->bt_buckets + csd->n_bts;)
	cr_hash_base = csa->acc_meth.bg.cache_state->cache_array + blk_hash;
	ocnt = 0;
	do
	{
		cr = cr_hash_base;
		assert((0 == cr->blk) || (BT_QUEHEAD == cr->blk));
		lcnt = hmax;
		do
		{
			cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.fl);
			assert(!CR_NOT_ALIGNED(cr, cr_low) && !CR_NOT_IN_RANGE(cr, cr_low, cr_high));
			if (BT_QUEHEAD == cr->blk)
			{	/* We have reached the end of the queue, validate we have run the queue
				 * back around to the same queue header or we'll need to retry because the
				 * queue changed on us.
				 */
				if (cr == cr_hash_base)
					return (cache_rec_ptr_t)NULL;
				break;			/* Retry - something changed */
			}
			if ((CR_BLKEMPTY != cr->blk) && ((cr->blk % hmax) != blk_hash))
				break;			/* Retry - something changed */
			assert(!csa->now_crit || (0 != cr->blkque.fl) && (0 != cr->blkque.bl));
			if (cr->blk == block)
			{
				if (CDB_STAGNATE <= t_tries || mu_reorg_process)
					CWS_INSERT(block);
				/* setting refer outside of crit may not prevent its replacement, but that's an
				 * inefficiency, not a tragedy because of concurrency checks in t_end or tp_tend;
				 * the real problem is to ensure that the cache_rec layout is such that this
				 * assignment does not damage other fields.
				 */
				cr->refer = TRUE;
				return cr;
			}
			lcnt--;
		} while (lcnt);
		ocnt++;
		/* We rarely expect to come here, hence it is considered better to recompute the maximum value of ocnt (for the
		 * termination check) instead of storing it in a local variable at the beginning of the do loop */
	} while (ocnt < (csa->now_crit ? 1 : ENOUGH_TRIES_TO_FALL_BACK));
	BG_TRACE_PRO_ANY(csa, db_csh_get_too_many_loops);
	return (TRUE == csa->now_crit ? (cache_rec_ptr_t)CR_NOTVALID : (cache_rec_ptr_t) NULL);
}
