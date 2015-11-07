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

#include <math.h> /* needed for handling of epoch_interval */
#include "gtm_time.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsdbver.h"
#include "gdsfhead.h"
#include "gdsblk.h"		/* needed for gdsblkops.h */
#include "gdscc.h"		/* needed for CDB_CW_SET_SIZE macro in gdsblkops.h */
#include "min_max.h"		/* needed for gdsblkops.h and MIN,MAX usage in this module */
#include "gdsblkops.h"
#include "filestruct.h"
#include "timers.h"
#include "mlkdef.h"
#include "collseq.h"
#include "iosp.h"
#include "jnl.h"
#include "gdsbml.h"
#include "mutex.h"
#include "mupip_exit.h"
#include "mucblkini.h"
#include "mucregini.h"
#include "gtmmsg.h"
#include "gtm_file_stat.h"
#include "lockconst.h"
#include "wcs_phase2_commit_wait.h"

#define WCR_SIZE_PER_BUF 	0
#define BLK_SIZE (((gd_segment*)gv_cur_region->dyn.addr)->blk_size)

GBLREF 	gd_region		*gv_cur_region;
GBLREF 	sgmnt_data_ptr_t	cs_data;
GBLREF 	sgmnt_addrs		*cs_addrs;

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_DBFILERR);
error_def(ERR_FILEPARSE);
error_def(ERR_GVIS);
error_def(ERR_MUNOACTION);
error_def(ERR_TEXT);

void mucregini(int4 blk_init_size)
{
	int4			status;
	int4			i;
	th_index_ptr_t 		th;
	collseq			*csp;
	uint4			ustatus;
	mstr 			jnlfile, jnldef, tmpjnlfile;
	time_t			ctime;

	MEMCPY_LIT(cs_data->label, GDS_LABEL);
	cs_data->desired_db_format = GDSVCURR;
	cs_data->fully_upgraded = TRUE;
	cs_data->db_got_to_v5_once = TRUE;	/* no V4 format blocks that are non-upgradeable */
	cs_data->minor_dbver = GDSMVCURR;
	cs_data->certified_for_upgrade_to = GDSVCURR;
	cs_data->creation_db_ver = GDSVCURR;
	cs_data->creation_mdb_ver = GDSMVCURR;
	cs_data->master_map_len = MASTER_MAP_SIZE_DFLT;
	cs_data->bplmap = BLKS_PER_LMAP;
	assert(BLK_SIZE <= MAX_DB_BLK_SIZE);
	cs_data->blk_size = BLK_SIZE;
	i = cs_data->trans_hist.total_blks;
	cs_data->trans_hist.free_blocks = i - DIVIDE_ROUND_UP(i, BLKS_PER_LMAP) - 2;
	cs_data->max_rec_size = gv_cur_region->max_rec_size;
	cs_data->max_key_size = gv_cur_region->max_key_size;
	cs_data->null_subs = gv_cur_region->null_subs;
	cs_data->std_null_coll = gv_cur_region->std_null_coll;
#ifdef UNIX
	cs_data->freeze_on_fail = gv_cur_region->freeze_on_fail;
	cs_data->mumps_can_bypass = gv_cur_region->mumps_can_bypass;
#endif
	cs_data->reserved_bytes = gv_cur_region->dyn.addr->reserved_bytes;
	cs_data->clustered = FALSE;
	cs_data->file_corrupt = 0;
	if (gv_cur_region->dyn.addr->lock_space)
		cs_data->lock_space_size = gv_cur_region->dyn.addr->lock_space * OS_PAGELET_SIZE;
	else
		cs_data->lock_space_size = DEF_LOCK_SIZE;
	NUM_CRIT_ENTRY(cs_data) = DEFAULT_NUM_CRIT_ENTRY;
	cs_data->staleness[0] = -300000000;	/* staleness timer = 30 seconds */
	cs_data->staleness[1] = -1;
	cs_data->ccp_quantum_interval[0] = -20000000;	/* 2 sec */
	cs_data->ccp_quantum_interval[1] = -1;
	cs_data->ccp_response_interval[0] = -600000000;	/* 1 min */
	cs_data->ccp_response_interval[1] = -1;
	cs_data->ccp_tick_interval[0] = -1000000;	/* 1/10 sec */
	cs_data->ccp_tick_interval[1] = -1;
	cs_data->last_com_backup = 1;
	cs_data->last_inc_backup = 1;
	cs_data->last_rec_backup = 1;
	cs_data->defer_time = gv_cur_region->dyn.addr->defer_time;
	cs_data->jnl_alq = gv_cur_region->jnl_alq;
	if (cs_data->jnl_state && !cs_data->jnl_alq)
		cs_data->jnl_alq = JNL_ALLOC_DEF;
	cs_data->jnl_deq = gv_cur_region->jnl_deq;
	cs_data->jnl_before_image = gv_cur_region->jnl_before_image;
	cs_data->jnl_state = gv_cur_region->jnl_state;
	cs_data->epoch_interval = JNL_ALLOWED(cs_data) ? DEFAULT_EPOCH_INTERVAL : 0;
	cs_data->alignsize = JNL_ALLOWED(cs_data) ? (DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE) : 0;
	ROUND_UP_JNL_BUFF_SIZE(cs_data->jnl_buffer_size, gv_cur_region->jnl_buffer_size, cs_data);
#ifdef UNIX
	if (JNL_ALLOWED(cs_data))
	{
		if (cs_data->jnl_alq + cs_data->jnl_deq > gv_cur_region->jnl_autoswitchlimit)
		{
			cs_data->autoswitchlimit = gv_cur_region->jnl_autoswitchlimit;
			cs_data->jnl_alq = cs_data->autoswitchlimit;
		} else
			cs_data->autoswitchlimit = ALIGNED_ROUND_DOWN(gv_cur_region->jnl_autoswitchlimit,
							cs_data->jnl_alq, cs_data->jnl_deq);
	}
	else
		cs_data->autoswitchlimit = 0;
	assert(!(MAX_IO_BLOCK_SIZE % DISK_BLOCK_SIZE));
	if (cs_data->jnl_alq + cs_data->jnl_deq > cs_data->autoswitchlimit)
		cs_data->jnl_alq = cs_data->autoswitchlimit;
#else
	cs_data->autoswitchlimit = JNL_ALLOWED(cs_data) ? ALIGNED_ROUND_DOWN(JNL_ALLOC_MAX, cs_data->jnl_alq, cs_data->jnl_deq) : 0;
#endif
	if (!cs_data->jnl_buffer_size)
		ROUND_UP_JNL_BUFF_SIZE(cs_data->jnl_buffer_size, JNL_BUFFER_DEF, cs_data);
	if (JNL_ALLOWED(cs_data))
		if (cs_data->jnl_buffer_size < JNL_BUFF_PORT_MIN(cs_data))
		{
			ROUND_UP_MIN_JNL_BUFF_SIZE(cs_data->jnl_buffer_size, cs_data);
		} else if (cs_data->jnl_buffer_size > JNL_BUFFER_MAX)
		{
			ROUND_DOWN_MAX_JNL_BUFF_SIZE(cs_data->jnl_buffer_size, cs_data);
		}
	cs_data->def_coll = gv_cur_region->def_coll;
	if (cs_data->def_coll)
	{
		if (csp = ready_collseq((int)(cs_data->def_coll)))
		{
			cs_data->def_coll_ver = (csp->version)(cs_data->def_coll);
			if (!do_verify(csp, cs_data->def_coll, cs_data->def_coll_ver))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_COLLTYPVERSION, 2, cs_data->def_coll, cs_data->def_coll_ver);
				mupip_exit(ERR_MUNOACTION);
			}
		} else
		{
			gtm_putmsg(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, cs_data->def_coll);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	/* mupip_set_journal() relies on cs_data->jnl_file_len being 0 if cs_data->jnl_state is jnl_notallowed.
	 * Note that even though gv_cur_region->jnl_state is jnl_notallowed, gv_cur_region->jnl_file_len can be non-zero
	 */
	cs_data->jnl_file_len = JNL_ALLOWED(cs_data) ? gv_cur_region->jnl_file_len : 0;
	cs_data->reg_seqno = 1;
	VMS_ONLY(
		cs_data->resync_seqno = 1;
		cs_data->old_resync_seqno = 1;
		cs_data->resync_tn = 1;
	)
	UNIX_ONLY(
		/* zqgblmod_seqno is initialized to 0 at db creation time (to ensure that $ZQGBLMOD will unconditionally return
		 * the safe value of TRUE by default). This default value of 0 is also relied upon by the source server logic
		 * when updating this as part of a fetchresync rollback. Initialize zqgblmod_tn to 0 to correspond to the seqno.
		 */
		cs_data->zqgblmod_seqno = 0;
		cs_data->zqgblmod_tn = 0;
		assert(!cs_data->multi_site_open);
		cs_data->multi_site_open = TRUE;
	)
	cs_data->repl_state = repl_closed;              /* default */
	if (cs_data->jnl_file_len)
	{
		tmpjnlfile.addr = (char *)cs_data->jnl_file_name;
		tmpjnlfile.len = SIZEOF(cs_data->jnl_file_name);
		jnlfile.addr = (char *)gv_cur_region->jnl_file_name;
		jnlfile.len = gv_cur_region->jnl_file_len;
		jnldef.addr = JNL_EXT_DEF;
		jnldef.len = SIZEOF(JNL_EXT_DEF) - 1;
		if (FILE_STAT_ERROR == gtm_file_stat(&jnlfile, &jnldef, &tmpjnlfile, TRUE, &ustatus))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, JNL_LEN_STR(gv_cur_region), ustatus);
			mupip_exit(ERR_MUNOACTION);
		}
		cs_data->jnl_file_len = tmpjnlfile.len;
	}
	cs_data->reserved_for_upd = UPD_RESERVED_AREA;
	cs_data->avg_blks_per_100gbl =  AVG_BLKS_PER_100_GBL;
	cs_data->pre_read_trigger_factor = PRE_READ_TRIGGER_FACTOR;
	cs_data->writer_trigger_factor = UPD_WRITER_TRIGGER_FACTOR;
	cs_data->db_trigger_cycle = 0;
	cs_addrs->hdr = cs_data;
	cs_addrs->ti = &cs_data->trans_hist;
	th = cs_addrs->ti;
	th->lock_sequence = 0;
	th->ccp_jnl_filesize = 0;
	cs_data->max_bts = GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS);
	cs_data->n_bts = BT_FACTOR(gv_cur_region->dyn.addr->global_buffers);
	cs_data->bt_buckets = getprime(cs_data->n_bts);

	cs_data->n_wrt_per_flu = 7;
	cs_data->flush_trigger = FLUSH_FACTOR(cs_data->n_bts);

	cs_data->max_update_array_size = cs_data->max_non_bm_update_array_size
				       = (int4)ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(cs_data), UPDATE_ARRAY_ALIGN_SIZE);
	cs_data->max_update_array_size += (int4)ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
	/* bt_malloc(cs_addrs) Done by db_init at file open time -- not needed here */
	if (dba_bg == gv_cur_region->dyn.addr->acc_meth)
		cs_data->flush_time[0] = TIM_FLU_MOD_BG;
	else
		cs_data->flush_time[0] = TIM_FLU_MOD_MM;
	cs_data->flush_time[1] = -1;
	cs_data->yield_lmt = DEFAULT_YIELD_LIMIT;
	cs_data->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	cs_data->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	cs_data->mutex_spin_parms.mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
	cs_data->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
	time(&ctime);
	assert(SIZEOF(ctime) >= SIZEOF(int4));
	cs_data->creation_time4 = (int4)ctime;	/* Need only lower order 4-bytes of current time (in case system time is 8-bytes) */
	cs_addrs->bmm = MM_ADDR(cs_data);
	bmm_init();
	for (i = 0; i < blk_init_size ; i += cs_data->bplmap)
	{	status = bml_init(i);
		if (status != SS_NORMAL)
		{
			gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, gv_cur_region->dyn.addr->fname_len,
					gv_cur_region->dyn.addr->fname, status);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	mucblkini();
	th->mm_tn = 0;
	th->early_tn = 1;
	th->curr_tn = 1;	/* in order to use INCREMENT_CURR_TN macro here, the logic has to be made complicated.
				 * this is because the macro relies on max_tn/max_tn_warn being set and that does not happen
				 * until a few lines later. hence keeping it simple here by doing a plain assignment of curr_tn.
				 */
	cs_data->max_tn = MAX_TN_V6;
	SET_TN_WARN(cs_data, cs_data->max_tn_warn);
	SET_LATCH_GLOBAL(&cs_data->next_upgrd_warn.time_latch, LOCK_AVAILABLE);
}
