/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <errno.h>
#include <sys/sem.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"
#include "t_qread.h"
#include "dse.h"
#include "gtmmsg.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "t_abort.h"
#include "t_retry.h"
#include "t_end.h"
#include "wbox_test_init.h"
#include "error.h"
#include "t_recycled2free.h"
#include "cdb_sc.h"
#include "eintr_wrappers.h"
#include "gtmimagename.h"
#include "clear_cache_array.h"
#include "gtmio.h"

#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "copy.h"
#include "shmpool.h"
#include "do_semop.h"

GBLREF	uint4			process_id;

void clear_cache_array(sgmnt_addrs *csa, sgmnt_data_ptr_t csd, gd_region* reg, uint4 new_total, uint4 old_total)
{
	char			*err_msg;
	boolean_t		got_lock;
	cache_rec_ptr_t         cr;
	cache_rec_ptr_t         cr_lo, cr_top, hash_hdr;
	bt_rec_ptr_t		bt;
	node_local_ptr_t	cnl;
	unix_db_info    	*udi;
	int			semval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If called from db_init, assure we've grabbed the access semaphor and are the only process attached to the database.
	 * Otherwise, we should have crit when called from wcs_recover or mu_truncate. */
	udi = FILE_INFO(reg);
	assert((udi->grabbed_access_sem && (1 == (semval = semctl(udi->semid, 1, GETVAL)))) || csa->now_crit);
	hash_hdr = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array;
	cr_lo = hash_hdr + csd->bt_buckets;
	cr_top = cr_lo + csd->n_bts;
	cnl = csa->nl;
	/* CLear all the cached blocks(FREE AND RECYCLED)
	 * that are greater than the post-truncate total_blks
	 * Done in crit, near the end of truncate.
	 */
	for (cr = cr_lo; cr < cr_top; cr++)
	{
		if (CR_BLKEMPTY != cr->blk && cr->blk >= new_total)
		{
			if ((0 == cr->dirty)
				VMS_ONLY(|| ((0 != cr->iosb.cond) && (0 == cr->bt_index))))
			{	/* cache record has no valid buffer attached, or its contents
				 * are in the database, or it has a more recent twin so we don't
				 * even have to care how its write terminated
				 */
				cr->cycle++;
				if (CR_BLKEMPTY != cr->blk)
				{
					bt = bt_get(cr->blk);
					if (bt)
						bt->cache_index = CR_NOTVALID;
				}
				cr->blk = BT_NOTVALID;	/* -1 */
				/* when cr->blk is empty, ensure no bt points to this cache-record */
				cr->bt_index = 0;	/* offset to bt_rec */
				cr->data_invalid = 0;	/* process_id */
				cr->dirty = 0;
				cr->flushed_dirty_tn = 0; /* value of dirty at the time of flush */
				cr->in_tend = 0;
				/* release of a shmpool reformat block if the current cache record is pointing to it */
				SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr);
				WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
				VMS_ONLY(cr->iosb.cond = 0;)
				cr->jnl_addr = 0;
				cr->refer = FALSE;
				cr->stopped = FALSE;
				/* increment number of write cache records in free queue */
				cnl->wc_in_free++;
			}
		}
	}
}
