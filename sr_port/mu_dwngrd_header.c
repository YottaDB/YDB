/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*---------------------------------------------------------------------------
	mu_dwngrd_header.c
	---------------
        This program will downgrade v4.x header to v3.x database.
 ----------------------------------------------------------------------------*/

#include "mdef.h"

#include <unistd.h>
#include <sys/stat.h>
#include "gtm_string.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v3_gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "timers.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "util.h"
#include "mu_dwngrd_header.h"

#if defined(UNIX)
#define GDS_V3_LABEL "GDSDYNUNX01"
#elif defined(VMS)
#define GDS_V3_LABEL "GDSDYNSEG09"
#endif


#define FLUSH 1


/* to keep old values instead of reinitialize them */
#define DWNGRADE_BG_TRC_REC(field) (new_head->field).evnt_cnt = (old_head->field).evnt_cnt; \
				  (new_head->field).evnt_tn  = (old_head->field).evnt_tn

/* is redundant */
#define INIT_BG_TRC_REC(field) (new_head->field).evnt_cnt = 0; \
			       (new_head->field).evnt_tn  = 0

/* If sizeof(old_head->field) <= sizeof(new_head->field), and same type and array */
#define DWNGRADE_MEM(field) memcpy(&new_head->field[0], &old_head->field[0], sizeof(new_head->field))





/*---------------------------------------------------------------------------
     Downgrade header from v4.x to v3.x.  This will cause header to shrink.
     Different between 32016E and others 3.x
	wc_blocked_t_end_hist/wc_blocked_t_end_hist2
     Do not use followings of 32016E
	jnl_blocked_writer_lost/jnl_blocked_writer_stuck/jnl_blocked_writer_blocked (for VMS)
 ---------------------------------------------------------------------------*/
void mu_dwngrd_header(sgmnt_data *old_head, v3_sgmnt_data *new_head)
{
	int4 old_hdr_size, new_hdr_size, old_hdr_size_vbn, new_hdr_size_vbn;

	old_hdr_size  = sizeof(*old_head);
	new_hdr_size = sizeof(*new_head);
	old_hdr_size_vbn = DIVIDE_ROUND_UP(old_hdr_size, DISK_BLOCK_SIZE);
	new_hdr_size_vbn = DIVIDE_ROUND_UP(new_hdr_size, DISK_BLOCK_SIZE);


	memcpy(new_head, GDS_V3_LABEL, GDS_LABEL_SZ);
	new_head->n_bts = old_head->n_bts;
	assert(old_head->acc_meth == dba_mm || old_head->acc_meth == dba_bg);
	new_head->acc_meth = old_head->acc_meth;
	new_head->start_vbn = old_head->start_vbn;
	new_head->createinprogress = old_head->createinprogress;
	new_head->file_corrupt = old_head->file_corrupt;
	new_head->total_blks_filler = old_head->total_blks_filler;
	new_head->created = old_head->created;
	new_head->lkwkval = old_head->lkwkval;
	if (old_head->lock_space_size <= (new_head->start_vbn - 1 - new_hdr_size_vbn) * DISK_BLOCK_SIZE)
		/* We have enough physical space for locks */
		new_head->lock_space_size = old_head->lock_space_size;
	else
	{
		/* We do not have enough physical space for locks */
		new_head->lock_space_size = (new_head->start_vbn - 1 - new_hdr_size_vbn) * DISK_BLOCK_SIZE;
		util_out_print("Lock space size is trucated to !UL bytes!!",FLUSH, new_head->lock_space_size);
	}
        new_head->free_space = (new_head->start_vbn - 1) * DISK_BLOCK_SIZE - new_hdr_size - new_head->lock_space_size;
	assert (new_head->free_space >= 0);
	new_head->owner_node = old_head->owner_node;
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
	DWNGRADE_MEM(n_retries);
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
	DWNGRADE_MEM(ccp_tick_interval);
	new_head->flu_outstanding = old_head->flu_outstanding;
	new_head->free_blocks_filler = old_head->free_blocks_filler;
	new_head->last_rec_backup = old_head->last_rec_backup;
	DWNGRADE_MEM(ccp_quantum_interval);
	DWNGRADE_MEM(ccp_response_interval);
	new_head->jnl_alq = old_head->jnl_alq;
	new_head->jnl_deq = old_head->jnl_deq;
	new_head->jnl_buffer_size = old_head->jnl_buffer_size;
	new_head->jnl_before_image = old_head->jnl_before_image;
	new_head->jnl_state = old_head->jnl_state;
	/* new_head->filler_glob_sec_init[0] */
	new_head->jnl_file_len = old_head->jnl_file_len;
	DWNGRADE_MEM(jnl_file_name);

	new_head->trans_hist.curr_tn = old_head->trans_hist.curr_tn;
	new_head->trans_hist.early_tn = old_head->trans_hist.early_tn;
	new_head->trans_hist.header_open_tn = old_head->trans_hist.header_open_tn;
	new_head->trans_hist.mm_tn = old_head->trans_hist.mm_tn;
	new_head->trans_hist.lock_sequence = old_head->trans_hist.lock_sequence;
	new_head->trans_hist.ccp_jnl_filesize = old_head->trans_hist.ccp_jnl_filesize;
	new_head->trans_hist.total_blks = old_head->trans_hist.total_blks;
	new_head->trans_hist.free_blocks = old_head->trans_hist.free_blocks;
	new_head->cache_lru_cycle = 0; 	/* assigned in run time */
	new_head->reserved_bytes = old_head->reserved_bytes;
	/* new_head->in_wtstart = 0; */
	new_head->def_coll = old_head->def_coll;
	new_head->def_coll_ver = old_head->def_coll_ver;
	new_head->image_count = old_head->image_count;
	new_head->freeze = old_head->freeze;
	new_head->rc_srv_cnt = old_head->rc_srv_cnt;
	new_head->dsid = old_head->dsid;
	new_head->rc_node = old_head->rc_node;
        /* new_head->dbfid;   Platform dependent. So updat in main */
	/*  filler2_char[16];  */


#if defined(VMS)
	DWNGRADE_BG_TRC_REC(rmv_free);
	DWNGRADE_BG_TRC_REC(rmv_clean);
	DWNGRADE_BG_TRC_REC(clean_to_mod);
	DWNGRADE_BG_TRC_REC(qio_to_mod);
	DWNGRADE_BG_TRC_REC(blocked);
	DWNGRADE_BG_TRC_REC(blkd_made_empty);
	DWNGRADE_BG_TRC_REC(obsolete_to_empty);
	DWNGRADE_BG_TRC_REC(qio_to_clean);
	DWNGRADE_BG_TRC_REC(stale);
	DWNGRADE_BG_TRC_REC(starved);
	DWNGRADE_BG_TRC_REC(active_lvl_trigger);
	DWNGRADE_BG_TRC_REC(new_buff);
	DWNGRADE_BG_TRC_REC(get_new_buff);
	DWNGRADE_BG_TRC_REC(mod_to_mod);
#endif
	DWNGRADE_BG_TRC_REC(db_csh_getn_flush_dirty);
	DWNGRADE_BG_TRC_REC(db_csh_getn_rip_wait);
	DWNGRADE_BG_TRC_REC(db_csh_getn_buf_owner_stuck);
	DWNGRADE_BG_TRC_REC(db_csh_getn_out_of_design);
	DWNGRADE_BG_TRC_REC(t_qread_buf_owner_stuck);
	DWNGRADE_BG_TRC_REC(t_qread_out_of_design);
	DWNGRADE_BG_TRC_REC(bt_put_flush_dirty);
	DWNGRADE_BG_TRC_REC(wc_blocked_wcs_verify_passed);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_qread_db_csh_getn_invalid_blk);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_qread_db_csh_get_invalid_blk);
	DWNGRADE_BG_TRC_REC(wc_blocked_db_csh_getn_loopexceed);
	DWNGRADE_BG_TRC_REC(wc_blocked_db_csh_getn_wcsstarvewrt);
	DWNGRADE_BG_TRC_REC(wc_blocked_db_csh_get);
	DWNGRADE_BG_TRC_REC(wc_blocked_tp_tend_wcsgetspace);
	DWNGRADE_BG_TRC_REC(wc_blocked_tp_tend_t1);
	DWNGRADE_BG_TRC_REC(wc_blocked_tp_tend_bitmap);
	DWNGRADE_BG_TRC_REC(wc_blocked_tp_tend_jnl_cwset);
	DWNGRADE_BG_TRC_REC(wc_blocked_tp_tend_jnl_wcsflu);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_hist1_nullbt);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_hist1_nonnullbt);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_bitmap_nullbt);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_bitmap_nonnullbt);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_jnl_cwset);
	DWNGRADE_BG_TRC_REC(wc_blocked_t_end_jnl_wcsflu);


	DWNGRADE_MEM(master_map);
}

