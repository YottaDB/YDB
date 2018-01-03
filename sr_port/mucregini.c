/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define PUTMSG_WARN_CSA(MSGPARMS)			\
MBSTART {						\
	if (IS_MUPIP_IMAGE)				\
		gtm_putmsg_csa MSGPARMS;		\
	else						\
		send_msg_csa MSGPARMS;			\
} MBEND

#define PUTMSG_ERROR_CSA(MSGPARMS)			\
MBSTART {						\
	if (IS_MUPIP_IMAGE)				\
		gtm_putmsg_csa MSGPARMS;		\
	else						\
		rts_error_csa MSGPARMS;			\
} MBEND

GBLREF 	gd_region		*gv_cur_region;
GBLREF 	sgmnt_data_ptr_t	cs_data;
GBLREF 	sgmnt_addrs		*cs_addrs;
GBLREF	void			(*mupip_exit_fp)(int4 errnum);

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_DBFILERR);
error_def(ERR_FILEPARSE);
error_def(ERR_GVIS);
error_def(ERR_JNLALLOCGROW);
error_def(ERR_MUNOACTION);
error_def(ERR_TEXT);

void mucregini(int4 blk_init_size)
{
	int4			status;
	int4			i;
	th_index_ptr_t 		th;
	collseq			*csp;
	uint4			ustatus, reg_autoswitch;
	mstr 			jnlfile, jnldef, tmpjnlfile;
	time_t			ctime;
	gd_region		*baseDBreg;
	gd_segment		*seg;
	sgmnt_data_ptr_t	csd;

	csd = cs_data;
	MEMCPY_LIT(csd->label, GDS_LABEL);
	csd->desired_db_format = GDSVCURR;
	csd->fully_upgraded = TRUE;
	csd->db_got_to_v5_once = TRUE;	/* no V4 format blocks that are non-upgradeable */
	csd->minor_dbver = GDSMVCURR;
	csd->certified_for_upgrade_to = GDSVCURR;
	csd->creation_db_ver = GDSVCURR;
	csd->creation_mdb_ver = GDSMVCURR;
	csd->master_map_len = MASTER_MAP_SIZE_DFLT;
	csd->bplmap = BLKS_PER_LMAP;
	seg = gv_cur_region->dyn.addr;
	assert(seg->blk_size <= MAX_DB_BLK_SIZE);
	csd->blk_size = seg->blk_size;
	i = csd->trans_hist.total_blks;
	csd->trans_hist.free_blocks = i - DIVIDE_ROUND_UP(i, BLKS_PER_LMAP) - 2;
	csd->max_rec_size = gv_cur_region->max_rec_size;
	csd->max_key_size = gv_cur_region->max_key_size;
	csd->null_subs = gv_cur_region->null_subs;
	csd->std_null_coll = gv_cur_region->std_null_coll;
	csd->freeze_on_fail = gv_cur_region->freeze_on_fail;
	csd->mumps_can_bypass = gv_cur_region->mumps_can_bypass;
	csd->epoch_taper = gv_cur_region->epoch_taper;
	csd->epoch_taper_time_pct = EPOCH_TAPER_TIME_PCT_DEFAULT;
	csd->epoch_taper_jnl_pct = EPOCH_TAPER_JNL_PCT_DEFAULT;
	csd->asyncio = IS_AIO_ON_SEG(seg);
	csd->reserved_bytes = seg->reserved_bytes;
	csd->clustered = FALSE;
	csd->lock_crit_with_db = gv_cur_region->lock_crit_with_db;
	csd->file_corrupt = 0;
	if (seg->lock_space)
		csd->lock_space_size = seg->lock_space * OS_PAGELET_SIZE;
	else
		csd->lock_space_size = DEF_LOCK_SIZE;
	csd->staleness[0] = -300000000;	/* staleness timer = 30 seconds */
	csd->staleness[1] = -1;
	csd->ccp_quantum_interval[0] = -20000000;	/* 2 sec */
	csd->ccp_quantum_interval[1] = -1;
	csd->ccp_response_interval[0] = -600000000;	/* 1 min */
	csd->ccp_response_interval[1] = -1;
	csd->ccp_tick_interval[0] = -1000000;	/* 1/10 sec */
	csd->ccp_tick_interval[1] = -1;
	csd->last_com_backup = 1;
	csd->last_inc_backup = 1;
	csd->last_rec_backup = 1;
	csd->defer_time = seg->defer_time;
	csd->jnl_alq = gv_cur_region->jnl_alq;
	if (csd->jnl_state && !csd->jnl_alq)
		csd->jnl_alq = JNL_ALLOC_DEF;
	csd->jnl_deq = gv_cur_region->jnl_deq;
	csd->jnl_before_image = gv_cur_region->jnl_before_image;
	csd->jnl_state = gv_cur_region->jnl_state;
	csd->epoch_interval = JNL_ALLOWED(csd) ? DEFAULT_EPOCH_INTERVAL : 0;
	csd->alignsize = JNL_ALLOWED(csd) ? (DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE) : 0;
	ROUND_UP_JNL_BUFF_SIZE(csd->jnl_buffer_size, gv_cur_region->jnl_buffer_size, csd);
	if (JNL_ALLOWED(csd))
	{
		reg_autoswitch = gv_cur_region->jnl_autoswitchlimit;
		if (csd->jnl_alq + csd->jnl_deq > reg_autoswitch)
		{
			PUTMSG_WARN_CSA((CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLALLOCGROW, 6, csd->jnl_alq,
					 gv_cur_region->jnl_autoswitchlimit, "database file", DB_LEN_STR(gv_cur_region)));
			csd->autoswitchlimit = reg_autoswitch;
			csd->jnl_alq = csd->autoswitchlimit;
		} else
		{
			csd->autoswitchlimit = ALIGNED_ROUND_DOWN(reg_autoswitch, csd->jnl_alq, csd->jnl_deq);
			/* If rounding down took us to less than the minimum autoswitch, then bump allocation to be
			 * equal to the pre-round-down autoswitchlimit this way all values are above their respective
			 * minimums. Note that the extension can be an arbitrary value because alloc == autoswitch.
			 */
			if (JNL_AUTOSWITCHLIMIT_MIN > csd->autoswitchlimit)
			{
				PUTMSG_WARN_CSA((CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLALLOCGROW, 6, csd->jnl_alq,
						 reg_autoswitch, "database file", DB_LEN_STR(gv_cur_region)));
				csd->jnl_alq = reg_autoswitch;
				csd->autoswitchlimit = reg_autoswitch;
			}
		}
	} else
		csd->autoswitchlimit = 0;
	assert(!(MAX_IO_BLOCK_SIZE % DISK_BLOCK_SIZE));
	if (csd->jnl_alq + csd->jnl_deq > csd->autoswitchlimit)
		csd->jnl_alq = csd->autoswitchlimit;
	if (!csd->jnl_buffer_size)
		ROUND_UP_JNL_BUFF_SIZE(csd->jnl_buffer_size, JNL_BUFFER_DEF, csd);
	if (JNL_ALLOWED(csd))
		if (csd->jnl_buffer_size < JNL_BUFF_PORT_MIN(csd))
		{
			ROUND_UP_MIN_JNL_BUFF_SIZE(csd->jnl_buffer_size, csd);
		} else if (csd->jnl_buffer_size > JNL_BUFFER_MAX)
		{
			ROUND_DOWN_MAX_JNL_BUFF_SIZE(csd->jnl_buffer_size, csd);
		}
	csd->def_coll = gv_cur_region->def_coll;
	if (csd->def_coll)
	{
		if (csp = ready_collseq((int)(csd->def_coll)))
		{
			csd->def_coll_ver = (csp->version)(csd->def_coll);
			if (!do_verify(csp, csd->def_coll, csd->def_coll_ver))
			{
				PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs)
						  VARLSTCNT(4) ERR_COLLTYPVERSION, 2, csd->def_coll, csd->def_coll_ver));
				assert(IS_MUPIP_IMAGE);
				(*mupip_exit_fp)(ERR_MUNOACTION);
			}
		} else
		{
			PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, csd->def_coll));
			assert(IS_MUPIP_IMAGE);
			(*mupip_exit_fp)(ERR_MUNOACTION);
		}
	}
	/* mupip_set_journal() relies on csd->jnl_file_len being 0 if csd->jnl_state is jnl_notallowed.
	 * Note that even though gv_cur_region->jnl_state is jnl_notallowed, gv_cur_region->jnl_file_len can be non-zero
	 */
	csd->jnl_file_len = JNL_ALLOWED(csd) ? gv_cur_region->jnl_file_len : 0;
	csd->reg_seqno = 1;
	/* zqgblmod_seqno is initialized to 0 at db creation time (to ensure that $ZQGBLMOD will unconditionally return
	 * the safe value of TRUE by default). This default value of 0 is also relied upon by the source server logic
	 * when updating this as part of a fetchresync rollback. Initialize zqgblmod_tn to 0 to correspond to the seqno.
	 */
	csd->zqgblmod_seqno = 0;
	csd->zqgblmod_tn = 0;
	assert(!csd->multi_site_open);
	csd->multi_site_open = TRUE;
	csd->repl_state = repl_closed;              /* default */
	if (csd->jnl_file_len)
	{
		tmpjnlfile.addr = (char *)csd->jnl_file_name;
		tmpjnlfile.len = SIZEOF(csd->jnl_file_name);
		jnlfile.addr = (char *)gv_cur_region->jnl_file_name;
		jnlfile.len = gv_cur_region->jnl_file_len;
		jnldef.addr = JNL_EXT_DEF;
		jnldef.len = SIZEOF(JNL_EXT_DEF) - 1;
		if (FILE_STAT_ERROR == gtm_file_stat(&jnlfile, &jnldef, &tmpjnlfile, TRUE, &ustatus))
		{
			PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_FILEPARSE, 2, JNL_LEN_STR(gv_cur_region), ustatus));
			assert(IS_MUPIP_IMAGE);
			(*mupip_exit_fp)(ERR_MUNOACTION);
		}
		csd->jnl_file_len = tmpjnlfile.len;
	}
	csd->reserved_for_upd = UPD_RESERVED_AREA;
	csd->avg_blks_per_100gbl =  AVG_BLKS_PER_100_GBL;
	csd->pre_read_trigger_factor = PRE_READ_TRIGGER_FACTOR;
	csd->writer_trigger_factor = UPD_WRITER_TRIGGER_FACTOR;
	csd->db_trigger_cycle = 0;
	cs_addrs->hdr = csd;
	cs_addrs->ti = &csd->trans_hist;
	th = cs_addrs->ti;
	th->lock_sequence = 0;
	th->ccp_jnl_filesize = 0;
	csd->max_bts = GTM64_ONLY(GTM64_WC_MAX_BUFFS) NON_GTM64_ONLY(WC_MAX_BUFFS);
	csd->n_bts = BT_FACTOR(seg->global_buffers);
	csd->bt_buckets = getprime(csd->n_bts);

	csd->n_wrt_per_flu = 7;
	csd->flush_trigger = FLUSH_FACTOR(csd->n_bts);

	csd->max_update_array_size = csd->max_non_bm_update_array_size
				       = (int4)ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE);
	csd->max_update_array_size += (int4)ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
	/* bt_malloc(cs_addrs) Done by db_init at file open time -- not needed here */
	if (dba_bg == REG_ACC_METH(gv_cur_region))
		csd->flush_time[0] = TIM_FLU_MOD_BG;
	else
		csd->flush_time[0] = TIM_FLU_MOD_MM;
	csd->flush_time[1] = -1;
	csd->yield_lmt = DEFAULT_YIELD_LIMIT;
	csd->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	csd->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	csd->mutex_spin_parms.mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
	NUM_CRIT_ENTRY(csd) = seg->mutex_slots;
	csd->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
	csd->defer_allocate = seg->defer_allocate;
	csd->read_only = seg->read_only;
	time(&ctime);
	assert(SIZEOF(ctime) >= SIZEOF(int4));
	csd->creation_time4 = (int4)ctime;	/* Need only lower order 4-bytes of current time (in case system time is 8-bytes) */
	if (IS_STATSDB_REG(gv_cur_region))
	{	/* Copy basedb fname into statsdb file header (needed by MUPIP RUNDOWN -FILE statsdb-file-name) */
		STATSDBREG_TO_BASEDBREG(gv_cur_region, baseDBreg);
		COPY_BASEDB_FNAME_INTO_STATSDB_HDR(gv_cur_region, baseDBreg, csd);
		/* Assert that we never create a statsdb with NOSTATS in corresponding baseDB */
		assert(baseDBreg->open);
		assert(!(RDBF_NOSTATS & baseDBreg->reservedDBFlags));
	}
	csd->reservedDBFlags = gv_cur_region->reservedDBFlags;
	cs_addrs->bmm = MM_ADDR(csd);
	bmm_init();
	for (i = 0; i < blk_init_size ; i += csd->bplmap)
	{
		status = bml_init(i);
		if (status != SS_NORMAL)
		{
			PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status));
			assert(IS_MUPIP_IMAGE);
			(*mupip_exit_fp)(ERR_MUNOACTION);
		}
	}
	mucblkini();
	th->mm_tn = 0;
	th->early_tn = 1;
	th->curr_tn = 1;	/* in order to use INCREMENT_CURR_TN macro here, the logic has to be made complicated.
				 * this is because the macro relies on max_tn/max_tn_warn being set and that does not happen
				 * until a few lines later. hence keeping it simple here by doing a plain assignment of curr_tn.
				 */
	csd->max_tn = MAX_TN_V6;
	SET_TN_WARN(csd, csd->max_tn_warn);
	SET_LATCH_GLOBAL(&csd->next_upgrd_warn.time_latch, LOCK_AVAILABLE);
}
