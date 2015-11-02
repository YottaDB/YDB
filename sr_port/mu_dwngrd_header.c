/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
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

LITREF  char	gtm_release_name[];
LITREF  int4	gtm_release_name_len;

error_def(ERR_MUINFOUINT8);

/* Downgrade header from v5.0-000 to v4.x */
void mu_dwngrd_header(sgmnt_data *csd, v15_sgmnt_data *v15_csd)
{
	time_t	temp_time;

	memset(v15_csd, 0, SIZEOF(v15_sgmnt_data));
	MEMCPY_LIT(v15_csd->label, GDS_LABEL_GENERIC);
	MEMCPY_LIT((v15_csd->label + SIZEOF(GDS_LABEL_GENERIC) - 1), GDS_V40);
	v15_csd->blk_size = csd->blk_size;
	v15_csd->bplmap = csd->bplmap;
	v15_csd->start_vbn = csd->start_vbn;
	v15_csd->acc_meth = csd->acc_meth;
	v15_csd->max_bts = csd->max_bts;
	v15_csd->n_bts = csd->n_bts;
	v15_csd->bt_buckets = csd->bt_buckets;
	v15_csd->reserved_bytes = csd->reserved_bytes + BLK_HDR_INCREASE;
	v15_csd->max_rec_size = csd->max_rec_size;
	v15_csd->max_key_size = csd->max_key_size;
	v15_csd->lock_space_size = csd->lock_space_size;
	v15_csd->extension_size = csd->extension_size;
	v15_csd->def_coll = csd->def_coll;
	v15_csd->def_coll_ver = csd->def_coll_ver;
	v15_csd->std_null_coll = csd->std_null_coll;
	v15_csd->null_subs = csd->null_subs;
	v15_csd->free_space = csd->free_space;
	v15_csd->mutex_spin_parms.mutex_hard_spin_count = csd->mutex_spin_parms.mutex_hard_spin_count;
	v15_csd->mutex_spin_parms.mutex_sleep_spin_count = csd->mutex_spin_parms.mutex_sleep_spin_count;
	v15_csd->mutex_spin_parms.mutex_spin_sleep_mask = csd->mutex_spin_parms.mutex_spin_sleep_mask;
	v15_csd->max_update_array_size = csd->max_update_array_size;	/* This is filler for some early V4 versions */
	v15_csd->max_non_bm_update_array_size = csd->max_non_bm_update_array_size;/* This is filler for some early V4 versions */
	v15_csd->file_corrupt = csd->file_corrupt;
	v15_csd->createinprogress = csd->createinprogress;
	time(&temp_time);	/* No need to propagate previous value */
	v15_csd->creation.date_time = (v15_time_t)temp_time;
	v15_csd->last_inc_backup = (v15_trans_num)csd->last_inc_backup;
	v15_csd->last_com_backup = (v15_trans_num)csd->last_com_backup;
	v15_csd->last_rec_backup = (v15_trans_num)csd->last_rec_backup;
	v15_csd->reorg_restart_block = csd->reorg_restart_block;
	memcpy(v15_csd->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
	VMS_ONLY(v15_csd->owner_node = csd->owner_node;)
	v15_csd->image_count = csd->image_count;
	v15_csd->kill_in_prog = (csd->kill_in_prog + csd->abandoned_kills);
	v15_csd->trans_hist.curr_tn = (v15_trans_num) csd->trans_hist.curr_tn;
	v15_csd->trans_hist.early_tn = (v15_trans_num) csd->trans_hist.early_tn;
	v15_csd->trans_hist.last_mm_sync = (v15_trans_num) csd->trans_hist.last_mm_sync;
	v15_csd->trans_hist.header_open_tn = (v15_trans_num) csd->trans_hist.curr_tn;
	v15_csd->trans_hist.mm_tn = (v15_trans_num) csd->trans_hist.mm_tn;
	v15_csd->trans_hist.lock_sequence = csd->trans_hist.lock_sequence;
	v15_csd->trans_hist.total_blks = csd->trans_hist.total_blks;
	v15_csd->trans_hist.free_blocks = csd->trans_hist.free_blocks;
	v15_csd->flush_time[0] = csd->flush_time[0];
	v15_csd->flush_time[1] = csd->flush_time[1];
	v15_csd->flush_trigger = csd->flush_trigger;
	v15_csd->n_wrt_per_flu = csd->n_wrt_per_flu;
	v15_csd->wait_disk_space = csd->wait_disk_space;
	v15_csd->defer_time = csd->defer_time;
#ifdef UNIX
	v15_csd->semid = INVALID_SEMID;
	v15_csd->shmid = INVALID_SHMID;
	v15_csd->gt_sem_ctime.ctime = 0;
	v15_csd->gt_shm_ctime.ctime = 0;
#endif
	/* Note none of the counter fields are being carried over. An upgrade or downgrade will
	   implicitly set them to zero by not initializing them.
	*/
	v15_csd->staleness[0] = csd->staleness[0];
	v15_csd->staleness[1] = csd->staleness[1];
	v15_csd->ccp_tick_interval[0] = csd->ccp_tick_interval[0];
	v15_csd->ccp_tick_interval[1] = csd->ccp_tick_interval[1];
	v15_csd->ccp_quantum_interval[0] = csd->ccp_quantum_interval[0];
	v15_csd->ccp_quantum_interval[1] = csd->ccp_quantum_interval[1];
	v15_csd->ccp_response_interval[0] = csd->ccp_response_interval[0];
	v15_csd->ccp_response_interval[1] = csd->ccp_response_interval[1];
	v15_csd->ccp_jnl_before = csd->ccp_jnl_before;
	v15_csd->clustered = csd->clustered;
	v15_csd->unbacked_cache = csd->unbacked_cache;
	v15_csd->rc_srv_cnt = csd->rc_srv_cnt;
	v15_csd->dsid = csd->dsid;
	v15_csd->rc_node = csd->rc_node;

	v15_csd->reg_seqno = csd->reg_seqno;
	VMS_ONLY(
		v15_csd->resync_seqno = csd->resync_seqno;
		v15_csd->old_resync_seqno = csd->old_resync_seqno;
		v15_csd->resync_tn = csd->resync_tn;
	)
	UNIX_ONLY(
		v15_csd->resync_seqno = (v15_trans_num)0;
		if (0 == csd->zqgblmod_seqno)
		{	/* Special value 0 of zqgblmod_seqno in multisite version corresponds to resync seqno of 1 in dualsite */
			v15_csd->old_resync_seqno = 1;
			v15_csd->resync_tn = 1;
		} else
		{
			v15_csd->old_resync_seqno = csd->zqgblmod_seqno;
			v15_csd->resync_tn = (v15_trans_num)csd->zqgblmod_tn;
		}
	)
	if (REPL_WAS_ENABLED(csd))
		v15_csd->repl_state = repl_closed;
	else
		v15_csd->repl_state = csd->repl_state;
	v15_csd->jnl_state = csd->jnl_state;
	if (JNL_ALLOWED(v15_csd))
	{
		v15_csd->jnl_alq = csd->jnl_alq;
		v15_csd->jnl_deq = csd->jnl_deq;
		v15_csd->jnl_buffer_size = csd->jnl_buffer_size;
		v15_csd->jnl_before_image = csd->jnl_before_image;
		v15_csd->jnl_file_len = csd->jnl_file_len;
		v15_csd->autoswitchlimit = csd->autoswitchlimit;
		v15_csd->epoch_interval = csd->epoch_interval;
		v15_csd->alignsize = csd->alignsize;
		v15_csd->jnl_sync_io = csd->jnl_sync_io;
		v15_csd->yield_lmt = csd->yield_lmt;
		memcpy(v15_csd->jnl_file_name, csd->jnl_file_name, JNL_NAME_SIZE);
		PRINT_JNL_FIELDS(v15_csd);
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Resync sequence number"),
					&v15_csd->resync_seqno, &v15_csd->resync_seqno);
	}
	memcpy(v15_csd->reorg_restart_key, csd->reorg_restart_key, SIZEOF(csd->reorg_restart_key));
	memcpy(v15_csd->machine_name, csd->machine_name, MAX_MCNAMELEN);
	v15_csd->certified_for_upgrade_to = GDSV4;		/* ust re-certify to upgrade again */
	v15_csd->creation_db_ver = csd->creation_db_ver;	/* Retain creation major/minor version */
	v15_csd->creation_mdb_ver = csd->creation_mdb_ver;
}
