/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This program will upgrade v4.x header to v5.0-000 database. */


#include "mdef.h"

#include <math.h> /* needed for handling of epoch_interval */
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "iosp.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "gdsblk.h"           /* needed for gdsblkops.h */
#include "jnl.h"
#include "mu_upgrd_dngrd_hdr.h"
#include "gtmmsg.h"
#include "lockconst.h"
#include "wcs_phase2_commit_wait.h"

LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

error_def(ERR_MUINFOUINT8);

/* Update header from v4.x to v5.0-000 */
void mu_upgrd_header(v15_sgmnt_data *v15_csd, sgmnt_data *csd)
{
	time_t	ctime;
	seq_num	v15_reg_seqno;

	memset(csd, 0, SIZEOF(sgmnt_data));
	MEMCPY_LIT(csd->label, GDS_LABEL);
	csd->blk_size = v15_csd->blk_size;
	csd->bplmap = v15_csd->bplmap;
	csd->start_vbn = v15_csd->start_vbn;
	csd->acc_meth = v15_csd->acc_meth;
	csd->max_bts = v15_csd->max_bts;
	csd->n_bts = v15_csd->n_bts;
	csd->bt_buckets = v15_csd->bt_buckets;
	if (v15_csd->reserved_bytes > BLK_HDR_INCREASE)
		csd->reserved_bytes = v15_csd->reserved_bytes - BLK_HDR_INCREASE;
	csd->max_rec_size = v15_csd->max_rec_size;
	csd->max_key_size = v15_csd->max_key_size;
	csd->lock_space_size = v15_csd->lock_space_size;
	csd->extension_size = v15_csd->extension_size;
	csd->def_coll = v15_csd->def_coll;
	csd->def_coll_ver = v15_csd->def_coll_ver;
	csd->std_null_coll = v15_csd->std_null_coll;					/* New in V5.0-FT01 */
	csd->null_subs = v15_csd->null_subs;
	csd->free_space = v15_csd->free_space;
	csd->mutex_spin_parms.mutex_hard_spin_count = v15_csd->mutex_spin_parms.mutex_hard_spin_count;
	csd->mutex_spin_parms.mutex_sleep_spin_count = v15_csd->mutex_spin_parms.mutex_sleep_spin_count;
	csd->mutex_spin_parms.mutex_spin_sleep_mask = v15_csd->mutex_spin_parms.mutex_spin_sleep_mask;
	csd->max_update_array_size = v15_csd->max_update_array_size;			/* New from V4.0-001G */
	csd->max_non_bm_update_array_size = v15_csd->max_non_bm_update_array_size;	/* New from V4.0-001G */
	csd->file_corrupt = v15_csd->file_corrupt;
	csd->minor_dbver = GDSMVCURR;					/* New in V5.0-000 */
	csd->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;	/* New from V5.3-002 */
	csd->createinprogress = v15_csd->createinprogress;
	time(&ctime);
	assert(SIZEOF(ctime) >= SIZEOF(int4));
	csd->creation_time4 = (int4)ctime;/* No need to propagate previous value. Take only lower order 4-bytes of current time */
	csd->last_inc_backup = v15_csd->last_inc_backup;
	csd->last_com_backup = v15_csd->last_com_backup;
	csd->last_rec_backup = v15_csd->last_rec_backup;
	csd->reorg_restart_block = v15_csd->reorg_restart_block;		/* New from V4.2 */
	memcpy(csd->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
	VMS_ONLY(csd->owner_node = v15_csd->owner_node;)
	csd->image_count = v15_csd->image_count;
	csd->kill_in_prog = 0;
	csd->abandoned_kills = v15_csd->kill_in_prog;	/* assert to 0 ??? */
	csd->blks_to_upgrd = v15_csd->trans_hist.total_blks - v15_csd->trans_hist.free_blocks;	/* New in V5.0-000 */
	assert(csd->blks_to_upgrd);
	csd->tn_upgrd_blks_0 = 0;								/* New in V5.0-000 */
	csd->fully_upgraded = FALSE;								/* New in V5.0-000 */
	csd->desired_db_format = GDSVCURR;							/* New in V5.0-000 */
	csd->desired_db_format_tn = v15_csd->trans_hist.curr_tn - 1;				/* New in V5.0-000 */
	csd->reorg_db_fmt_start_tn = 0;								/* New in V5.0-000 */
	csd->certified_for_upgrade_to = v15_csd->certified_for_upgrade_to;			/* New in V5.0-000 */
	csd->master_map_len = MASTER_MAP_SIZE_V4;						/* New in V5.0-000 */
	csd->reorg_upgrd_dwngrd_restart_block = 0;						/* New in V5.0-000 */
	csd->creation_db_ver = v15_csd->creation_db_ver;	/* Retain creation major/minor version */
	csd->creation_mdb_ver = v15_csd->creation_mdb_ver;
	csd->trans_hist.early_tn = v15_csd->trans_hist.early_tn;
	csd->trans_hist.curr_tn = v15_csd->trans_hist.curr_tn;	/* INCREMENT_CURR_TN comment added to note curr_tn set is done */
	csd->max_tn = MAX_TN_V6;		/* New in V5.0-000 */
	SET_TN_WARN(csd, csd->max_tn_warn);	/* New in V5.0-000 */
	csd->trans_hist.last_mm_sync = v15_csd->trans_hist.last_mm_sync;
	csd->trans_hist.mm_tn = v15_csd->trans_hist.mm_tn;
	csd->trans_hist.lock_sequence = v15_csd->trans_hist.lock_sequence;
	csd->trans_hist.total_blks = v15_csd->trans_hist.total_blks;
	csd->trans_hist.free_blocks = v15_csd->trans_hist.free_blocks;
	csd->flush_time[0] = v15_csd->flush_time[0];
	csd->flush_time[1] = v15_csd->flush_time[1];
	csd->flush_trigger = v15_csd->flush_trigger;
	csd->n_wrt_per_flu = v15_csd->n_wrt_per_flu;
	csd->wait_disk_space = v15_csd->wait_disk_space;
	csd->defer_time = v15_csd->defer_time;
#ifdef UNIX
	csd->semid = INVALID_SEMID;
	csd->shmid = INVALID_SHMID;
	csd->gt_sem_ctime.ctime = 0;
	csd->gt_shm_ctime.ctime = 0;
#endif
	/* Note none of the counter fields are being carried over. An upgrade or downgrade will
	   implicitly set them to zero by not initializing them.
	*/
	csd->staleness[0] = v15_csd->staleness[0];
	csd->staleness[1] = v15_csd->staleness[1];
	csd->ccp_tick_interval[0] = v15_csd->ccp_tick_interval[0];
	csd->ccp_tick_interval[1] = v15_csd->ccp_tick_interval[1];
	csd->ccp_quantum_interval[0] = v15_csd->ccp_quantum_interval[0];
	csd->ccp_quantum_interval[1] = v15_csd->ccp_quantum_interval[1];
	csd->ccp_response_interval[0] = v15_csd->ccp_response_interval[0];
	csd->ccp_response_interval[1] = v15_csd->ccp_response_interval[1];
	csd->ccp_jnl_before = v15_csd->ccp_jnl_before;
	csd->clustered = v15_csd->clustered;
	csd->unbacked_cache = v15_csd->unbacked_cache;
	csd->rc_srv_cnt = v15_csd->rc_srv_cnt;
	csd->dsid = v15_csd->dsid;
	csd->rc_node = v15_csd->rc_node;
	v15_reg_seqno = csd->reg_seqno = v15_csd->reg_seqno;
	csd->repl_state = v15_csd->repl_state;
	VMS_ONLY(
		csd->resync_seqno = v15_csd->resync_seqno;
		csd->resync_tn = v15_csd->resync_tn;
		csd->old_resync_seqno = v15_csd->old_resync_seqno;
		/* resync_seqno should never be greater the region's reg_seqno. Ensure that this is indeed the case. In PRO,
		 * fix the fields to be at most the value of the region's reg_seqno if they are found to be greater than
		 * reg_seqno
		 */
		assert((csd->resync_seqno <= v15_reg_seqno) && (csd->old_resync_seqno <= v15_reg_seqno));
		if (csd->resync_seqno > v15_reg_seqno)
			csd->resync_seqno = v15_reg_seqno;
		if (csd->old_resync_seqno > v15_reg_seqno)
			csd->old_resync_seqno = v15_reg_seqno;

		assert(0 != csd->reg_seqno || (0 == csd->resync_seqno && 0 == csd->resync_tn && repl_closed == csd->repl_state));
		assert(0 != csd->resync_seqno || (0 == csd->reg_seqno && 0 == csd->resync_tn && repl_closed == csd->repl_state));
		assert(0 != csd->resync_tn || (0 == csd->reg_seqno && 0 == csd->resync_seqno && repl_closed == csd->repl_state));
		if (0 == csd->reg_seqno || 0 == csd->resync_seqno || 0 == csd->resync_tn)
		{	/* This can happen for pre-replication versions */
			csd->reg_seqno = 1;
			csd->resync_seqno = 1;
			csd->resync_tn = 1;
			csd->old_resync_seqno = 1;
			csd->repl_state = repl_closed;
		}
	)
	UNIX_ONLY(
		csd->zqgblmod_seqno = v15_csd->old_resync_seqno;
		csd->zqgblmod_tn = v15_csd->resync_tn;
		if (1 == csd->zqgblmod_seqno)
		{	/* Special value 1 of resync seqno in dualsite version corresponds to zqgblmod_seqno of 0 in multisite */
			csd->zqgblmod_seqno = 0;
			csd->zqgblmod_tn = 0;
		}
		assert(0 != csd->reg_seqno
			|| (0 == csd->zqgblmod_seqno && 0 == csd->zqgblmod_tn && repl_closed == csd->repl_state));
		if (0 == csd->reg_seqno)
		{	/* This can happen for pre-replication versions */
			csd->reg_seqno = 1;
			csd->zqgblmod_seqno = 0;	/* see comment in mucregini.c for why initial value is 0 */
			csd->zqgblmod_tn = 0;		/* see comment in mucregini.c for why initial value is 0 */
			csd->repl_state = repl_closed;
		} else
		{
			csd->pre_multisite_resync_seqno = v15_csd->resync_seqno;
			/* resync_seqno should never be greater the region's reg_seqno. Ensure that this is indeed the case. In PRO,
			 * fix the fields to be at most the value of the region's reg_seqno if they are found to be greater than
			 * reg_seqno
			 */
			assert(v15_csd->resync_seqno <= v15_reg_seqno);
		}
		assert(!csd->multi_site_open);
		csd->multi_site_open = TRUE;
	)
	csd->jnl_state = v15_csd->jnl_state;
	if (JNL_ALLOWED(csd))
	{
		csd->jnl_alq = v15_csd->jnl_alq;
		if (!csd->jnl_alq)
			csd->jnl_alq = JNL_ALLOC_DEF;
		csd->epoch_interval = v15_csd->epoch_interval;
		if (!csd->epoch_interval)
			csd->epoch_interval = DEFAULT_EPOCH_INTERVAL;
		csd->alignsize = v15_csd->alignsize;
		if (!csd->alignsize)
			csd->alignsize = DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE;
		csd->jnl_deq = v15_csd->jnl_deq;
		csd->autoswitchlimit = ALIGNED_ROUND_DOWN(JNL_ALLOC_MAX, csd->jnl_alq, csd->jnl_deq);
		csd->jnl_buffer_size = v15_csd->jnl_buffer_size;
		csd->jnl_before_image = v15_csd->jnl_before_image;
		csd->jnl_file_len = v15_csd->jnl_file_len;
		csd->jnl_sync_io = v15_csd->jnl_sync_io;
		csd->yield_lmt = v15_csd->yield_lmt;
		memcpy(csd->jnl_file_name, v15_csd->jnl_file_name, JNL_NAME_SIZE);
		PRINT_JNL_FIELDS(csd);
		VMS_ONLY(
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Resync sequence number"),
						&csd->resync_seqno, &csd->resync_seqno);
		)
	}
	memcpy(csd->reorg_restart_key, v15_csd->reorg_restart_key, SIZEOF(csd->reorg_restart_key));	/* New from V4.2 */
	memcpy(csd->machine_name, v15_csd->machine_name, MAX_MCNAMELEN);
	csd->reserved_for_upd = UPD_RESERVED_AREA;
	csd->avg_blks_per_100gbl =  AVG_BLKS_PER_100_GBL;
	csd->pre_read_trigger_factor = PRE_READ_TRIGGER_FACTOR;
	csd->writer_trigger_factor = UPD_WRITER_TRIGGER_FACTOR;
	SET_LATCH_GLOBAL(&csd->next_upgrd_warn.time_latch, LOCK_AVAILABLE);
}
