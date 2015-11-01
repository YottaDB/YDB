/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*---------------------------------------------------------------------------
	mu_upgrd_header.c
	---------------
        This program will upgrade v3.x header to v4.x database.
	note: Some operation in the header is redundant,
	      but this is to keep track of the fields in the file header.
 ----------------------------------------------------------------------------*/


#include "mdef.h"

#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "gtm_string.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v3_gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsblk.h"           /* needed for gdsblkops.h */
#include "gdscc.h"            /* needed for CDB_CW_SET_SIZE macro in gdsblkops.h */
#include "min_max.h"          /* needed for gdsblkops.h and MIN,MAX usage in this module */
#include "gdsblkops.h"
#include "timers.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "mu_upgrd.h"
#include "mu_upgrd_header.h"




/* to keep old values instead of reinitialize them */
#define UPGRADE_BG_TRC_REC(field) (new_head->field).evnt_cnt = (old_head->field).evnt_cnt; \
				  (new_head->field).evnt_tn  = (old_head->field).evnt_tn

/* is redundant */
#define INIT_BG_TRC_REC(field) (new_head->field).evnt_cnt = 0; \
			       (new_head->field).evnt_tn  = 0

/* If sizeof(old_head->field) <= sizeof(new_head->field), and same type and array */
#define UPGRADE_MEM(field) memcpy(&new_head->field[0], &old_head->field[0], sizeof(old_head->field))





/*---------------------------------------------------------------------------
     Update header from v3.x to v4.x.  This will cause header to expand.
     Might be recalculated in main:
     	1. start_vbn
	2. dbfid
	3. free_space
	4. wc_blocked_t_end_hist/wc_blocked_t_end_hist2 (different between 16F and others 3.x)
	5. flush_trigger/jnl_blocked_writer_lost/jnl_blocked_writer_stuck/jnl_blocked_writer_blocked (for VMS)
 ---------------------------------------------------------------------------*/
void mu_upgrd_header(v3_sgmnt_data *old_head, sgmnt_data *new_head)
{

	memcpy(new_head,GDS_LABEL, GDS_LABEL_SZ);
	new_head->n_bts = old_head->n_bts;
	assert(old_head->acc_meth == dba_mm || old_head->acc_meth == dba_bg);
	new_head->acc_meth = old_head->acc_meth;
	/* new_head->start_vbn : Different for UNIX/VMS. So do not calculate it here */
	new_head->createinprogress = old_head->createinprogress;
	new_head->file_corrupt = old_head->file_corrupt;
	new_head->total_blks_filler = old_head->total_blks_filler;
	new_head->created = old_head->created;
	new_head->lkwkval = old_head->lkwkval;
	new_head->lock_space_size = old_head->lock_space_size;
	new_head->owner_node = old_head->owner_node;
	/* new_head->free_space : Different for UNIX/VMS. So do not calculate it here */
	new_head->max_bts =  WC_MAX_BUFFS;
	new_head->extension_size = old_head->extension_size;
	new_head->blk_size = old_head->blk_size;
	new_head->max_rec_size = old_head->max_rec_size;
	new_head->max_key_size = old_head->max_key_size;
	new_head->null_subs = old_head->null_subs;
	new_head->lock_write = old_head->lock_write;
	new_head->ccp_jnl_before = old_head->ccp_jnl_before;
	new_head->clustered = old_head->clustered;
	new_head->flush_done = old_head->flush_done;
	new_head->unbacked_cache = old_head->unbacked_cache;
	new_head->bplmap = old_head->bplmap;
	new_head->bt_buckets = old_head->bt_buckets;
	new_head->n_wrt_per_flu = old_head->n_wrt_per_flu;
	UPGRADE_MEM(n_retries);
	new_head->n_puts = old_head->n_puts;
	new_head->n_kills = old_head->n_kills;
	new_head->n_queries = old_head->n_queries;
	new_head->n_gets = old_head->n_gets;
	new_head->n_order = old_head->n_order;
	new_head->n_zprevs = old_head->n_zprevs;
	new_head->n_data = old_head->n_data;
	new_head->wc_rtries = old_head->wc_rtries;
	new_head->wc_rhits = old_head->wc_rhits;
	/* new_head->wcs_staleness =  old_head->wcs_staleness; */
	new_head->wc_blocked = old_head->wc_blocked;
	new_head->root_level = old_head->root_level;
	/* new_head->filler_short */
	if (old_head->acc_meth == dba_bg)
		new_head->flush_time[0] = TIM_FLU_MOD_BG;
	else
		new_head->flush_time[0] = TIM_FLU_MOD_MM;
	new_head->flush_time[1] = -1;
	new_head->last_inc_backup = old_head->last_inc_backup;
	new_head->last_com_backup = old_head->last_com_backup;
	new_head->staleness[0] =  -300000000;
	new_head->staleness[1] =  -1;
	UPGRADE_MEM(ccp_tick_interval);
	new_head->flu_outstanding = old_head->flu_outstanding;
	new_head->free_blocks_filler = old_head->free_blocks_filler;
	memcpy((unsigned char *) (&new_head->jnl_file),
	       (unsigned char *) (&old_head->jnl_file), sizeof(old_head->jnl_file));
	new_head->last_rec_backup = old_head->last_rec_backup;
	UPGRADE_MEM(ccp_quantum_interval);
	UPGRADE_MEM(ccp_response_interval);
	new_head->jnl_alq = old_head->jnl_alq;
	new_head->jnl_deq = old_head->jnl_deq;
	new_head->jnl_buffer_size = old_head->jnl_buffer_size;
	new_head->jnl_before_image = old_head->jnl_before_image;
	new_head->jnl_state = old_head->jnl_state;
	/* new_head->filler_glob_sec_init[0] */
	new_head->jnl_file_len = old_head->jnl_file_len;
	UPGRADE_MEM(jnl_file_name);

	new_head->trans_hist.curr_tn = old_head->trans_hist.curr_tn;
	new_head->trans_hist.early_tn = old_head->trans_hist.early_tn;
	new_head->trans_hist.last_mm_sync = old_head->trans_hist.curr_tn;
	new_head->trans_hist.header_open_tn = old_head->trans_hist.header_open_tn;
	new_head->trans_hist.mm_tn = old_head->trans_hist.mm_tn;
	new_head->trans_hist.lock_sequence = old_head->trans_hist.lock_sequence;
	new_head->trans_hist.ccp_jnl_filesize = old_head->trans_hist.ccp_jnl_filesize;
	new_head->trans_hist.total_blks = old_head->trans_hist.total_blks;
	new_head->trans_hist.free_blocks = old_head->trans_hist.free_blocks;
	new_head->cache_lru_cycle = 0; 	/* assigned in run time */
	new_head->reserved_bytes = old_head->reserved_bytes;
	/* new_head->in_wtstart = 0; */
	new_head->defer_time = 1;
	new_head->def_coll = old_head->def_coll;
	new_head->def_coll_ver = old_head->def_coll_ver;
	new_head->image_count = old_head->image_count;
	new_head->freeze = old_head->freeze;
	new_head->rc_srv_cnt = old_head->rc_srv_cnt;
	new_head->dsid = old_head->dsid;
	new_head->rc_node = old_head->rc_node;
	time(&new_head->creation.date_time);	/* Set creation date/time to current */
        /* new_head->dbfid;   Platform dependent. So updat in main */
	/*  filler2_char[16];  */


#if defined(VMS)
	UPGRADE_BG_TRC_REC(rmv_free);
	UPGRADE_BG_TRC_REC(rmv_clean);
	UPGRADE_BG_TRC_REC(clean_to_mod);
	UPGRADE_BG_TRC_REC(qio_to_mod);
	UPGRADE_BG_TRC_REC(blocked);
	UPGRADE_BG_TRC_REC(blkd_made_empty);
	UPGRADE_BG_TRC_REC(obsolete_to_empty);
	UPGRADE_BG_TRC_REC(qio_to_clean);
	UPGRADE_BG_TRC_REC(stale);
	UPGRADE_BG_TRC_REC(starved);
	UPGRADE_BG_TRC_REC(active_lvl_trigger);
	UPGRADE_BG_TRC_REC(new_buff);
	UPGRADE_BG_TRC_REC(get_new_buff);
	UPGRADE_BG_TRC_REC(mod_to_mod);
#elif defined(UNIX)
	INIT_BG_TRC_REC(total_buffer_flush);
	INIT_BG_TRC_REC(bufct_buffer_flush);
	INIT_BG_TRC_REC(bufct_buffer_flush_loop);
	INIT_BG_TRC_REC(stale_timer_started);
	INIT_BG_TRC_REC(stale_timer_pop);
	INIT_BG_TRC_REC(stale_process_defer);
	INIT_BG_TRC_REC(stale_defer_processed);
	INIT_BG_TRC_REC(wrt_calls);
	INIT_BG_TRC_REC(wrt_count);
	INIT_BG_TRC_REC(wrt_blocked);
	INIT_BG_TRC_REC(wrt_busy);
	INIT_BG_TRC_REC(wrt_noblks_wrtn);
	INIT_BG_TRC_REC(reserved_bgtrcrec);
	INIT_BG_TRC_REC(lost_block_recovery);
#else
# error Unsupported platform
#endif
	INIT_BG_TRC_REC(spcfc_buffer_flush);
	INIT_BG_TRC_REC(spcfc_buffer_flush_loop);
	INIT_BG_TRC_REC(spcfc_buffer_flush_retries);
	INIT_BG_TRC_REC(spcfc_buffer_flushed_during_lockwait);
	INIT_BG_TRC_REC(tp_crit_retries);
	UPGRADE_BG_TRC_REC(db_csh_getn_flush_dirty);
	UPGRADE_BG_TRC_REC(db_csh_getn_rip_wait);
	UPGRADE_BG_TRC_REC(db_csh_getn_buf_owner_stuck);
	UPGRADE_BG_TRC_REC(db_csh_getn_out_of_design);
	UPGRADE_BG_TRC_REC(t_qread_buf_owner_stuck);
	UPGRADE_BG_TRC_REC(t_qread_out_of_design);
	UPGRADE_BG_TRC_REC(bt_put_flush_dirty);
	INIT_BG_TRC_REC(mlock_wakeups);
	UPGRADE_BG_TRC_REC(wc_blocked_wcs_verify_passed);
	UPGRADE_BG_TRC_REC(wc_blocked_t_qread_db_csh_getn_invalid_blk);
	UPGRADE_BG_TRC_REC(wc_blocked_t_qread_db_csh_get_invalid_blk);
	UPGRADE_BG_TRC_REC(wc_blocked_db_csh_getn_loopexceed);
	UPGRADE_BG_TRC_REC(wc_blocked_db_csh_getn_wcsstarvewrt);
	UPGRADE_BG_TRC_REC(wc_blocked_db_csh_get);
	UPGRADE_BG_TRC_REC(wc_blocked_tp_tend_wcsgetspace);
	UPGRADE_BG_TRC_REC(wc_blocked_tp_tend_t1);
	UPGRADE_BG_TRC_REC(wc_blocked_tp_tend_bitmap);
	UPGRADE_BG_TRC_REC(wc_blocked_tp_tend_jnl_cwset);
	UPGRADE_BG_TRC_REC(wc_blocked_tp_tend_jnl_wcsflu);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_hist1_nullbt);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_hist1_nonnullbt);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_bitmap_nullbt);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_bitmap_nonnullbt);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_jnl_cwset);
	UPGRADE_BG_TRC_REC(wc_blocked_t_end_jnl_wcsflu);
	INIT_BG_TRC_REC(db_csh_get_too_many_loops);
	/* filler_name_pad;  */
	INIT_BG_TRC_REC(wc_blocked_tpckh_hist1_nullbt);
	INIT_BG_TRC_REC(wc_blocked_tpckh_hist1_nonnullbt);
	/* filler_2k[896];   */

#ifdef UNIX
	new_head->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	new_head->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	new_head->mutex_spin_parms.mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
	new_head->semid = INVALID_SEMID;
	new_head->shmid = INVALID_SHMID;
#endif
	/* mutex_filler1 */
	/* mutex_filler1 */
	/* mutex_filler1 */
	/* mutex_filler1 */
	/* mutex_filler1 */
	/* filler3[992]	 */
	/* filler_4k[1020]			  */
	/* new_head->unique_id[0] = 0;   	  */		/* assigned in run time */
	/* new_head->machine_name[0] = 0;  	  */		/* assigned in run time */
	new_head->flush_trigger =  FLUSH_FACTOR(new_head->n_bts); /* same as mucregeni.c */
	new_head->max_update_array_size = new_head->max_non_bm_update_array_size
                                       = ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(new_head), UPDATE_ARRAY_ALIGN_SIZE);
	new_head->max_update_array_size += ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
	/* filler_5k[732];  */
	/* filler_6k[1024]; */
	/* filler_7k[1024]; */
	/* filler_8k[1024]; */


	/* master map: v3.x has 2*DISK_BLOCK_SIZE  bytes and
	   this will be copied into the first 2*DISK_BLOCK_SIZE bytes
	   of 32*DISK_BLOCK_SIZE bytes master map of v4.x.
	   Remaining 30*DISK_BLOCK_SIZE bytes are initialized  to 0xff */
	UPGRADE_MEM(master_map);
	memset(&(new_head->master_map[2*DISK_BLOCK_SIZE]), 0xFF, (MASTER_MAP_BLOCKS-2)*DISK_BLOCK_SIZE);
}

