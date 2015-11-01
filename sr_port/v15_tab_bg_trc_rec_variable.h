/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note that each TAB_BG_TRC_REC entry corresponds to a field in the file-header
 * Additions are to be done at the END of the file. (this grows in the direction top to bottom)
 * Replacing existing fields with new fields is allowed (provided their implications are thoroughly analyzed).
 */

TAB_BG_TRC_REC("  WcBlkdtpc hist1 nul bt", wc_blocked_tpckh_hist1_nullbt)
TAB_BG_TRC_REC("  WcBlkdtpc hist1 no nul", wc_blocked_tpckh_hist1_nonnullbt)
TAB_BG_TRC_REC("  JnlBlkd Writer Lost   ", jnl_blocked_writer_lost)
TAB_BG_TRC_REC("  JnlBlkd Writer Stuck  ", jnl_blocked_writer_stuck)
TAB_BG_TRC_REC("  JnlBlkd Writer Blocked", jnl_blocked_writer_blocked)
TAB_BG_TRC_REC("  Journal fsyncs        ", n_jnl_fsyncs)		/* count of jnl fsync on to disk - unix only */
TAB_BG_TRC_REC("  Journal fsync tries   ", n_jnl_fsync_tries)		/* attempted jnl fsyncs  - unix only */
TAB_BG_TRC_REC("  Journal fsync recover ", n_jnl_fsync_recovers)	/* how many fsync recovers were done - unix only */
TAB_BG_TRC_REC("  DB fsyncs             ", n_db_fsyncs)			/* count of db fsyncs on to disk - unix only */
TAB_BG_TRC_REC("  DB fsyncs in crit     ", n_db_fsyncs_in_crit)		/* count of db fsyncs (in crit) - unix only */
TAB_BG_TRC_REC("  Epoch Timer Calls     ", n_dbsync_timers)		/* count of calls to wcs_clean_dbsync_timer */
TAB_BG_TRC_REC("  Epoch Timer Writes    ", n_dbsync_writes)		/* no. of dbsyncs actually done by wcs_clean_dbsync */
TAB_BG_TRC_REC("  Mutex Queue full      ", mutex_queue_full)		/* number of times the mutex queue overflowed */
TAB_BG_TRC_REC("  WcBlocked from bt_put ", wcb_bt_put)
TAB_BG_TRC_REC("  WcBlocked grab crit   ", wcb_grab_crit)
TAB_BG_TRC_REC("  WcBlocked tp_grab_crit", wcb_tp_grab_crit)
TAB_BG_TRC_REC("  WcBlocked nocr_invcr  ", wcb_t_end_sysops_nocr_invcr)
TAB_BG_TRC_REC("  WcBlocked cr_invcr    ", wcb_t_end_sysops_cr_invcr)
TAB_BG_TRC_REC("  WcBlocked rip_wait    ", wcb_t_end_sysops_rip_wait)
TAB_BG_TRC_REC("  WcBlocked dirtyripwait", wcb_t_end_sysops_dirtyripwait)
TAB_BG_TRC_REC("  WcBlocked gds_rundown ", wcb_gds_rundown)
TAB_BG_TRC_REC("  WcBlocked wcs_flu1    ", wcb_wcs_flu1)
TAB_BG_TRC_REC("  WcBlocked mu_backup   ", wcb_mu_back)
TAB_BG_TRC_REC("  WcBlocked dirty_invcr ", wcb_t_end_sysops_dirty_invcr)
TAB_BG_TRC_REC("  WcBlocked wtfini_fail ", wcb_t_end_sysops_wtfini_fail)
TAB_BG_TRC_REC("  WcBlocked twin_stuck  ", wcb_t_end_sysops_twin_stuck)
TAB_BG_TRC_REC("  DbCshGetn_WrtLtchStuck", db_csh_getn_wrt_latch_stuck)
TAB_BG_TRC_REC("  WcBlocked secshrclnup1", wcb_secshr_db_clnup_now_crit)
TAB_BG_TRC_REC("  WcBlocked t_qread_bt1 ", wc_blocked_t_qread_bad_bt_index1)
TAB_BG_TRC_REC("  WcBlocked t_qread_bt2 ", wc_blocked_t_qread_bad_bt_index2)
TAB_BG_TRC_REC("  WcBlocked tend crbt1  ", wc_blocked_t_end_crbtmismatch1)
TAB_BG_TRC_REC("  WcBlocked tend crbt2  ", wc_blocked_t_end_crbtmismatch2)
TAB_BG_TRC_REC("  WcBlocked tend crbt3  ", wc_blocked_t_end_crbtmismatch3)
TAB_BG_TRC_REC("  WcBlocked tptend crbt1", wc_blocked_tp_tend_crbtmismatch1)
TAB_BG_TRC_REC("  WcBlocked tptend crbt2", wc_blocked_tp_tend_crbtmismatch2)
TAB_BG_TRC_REC("  WcBlocked tptend crbt3", wc_blocked_tp_tend_crbtmismatch3)
TAB_BG_TRC_REC("  WcBlocked wcs_wtstart ", wc_blocked_wcs_wtstart_bad_cr)
TAB_BG_TRC_REC("  WcBlocked wcs_wtfini  ", wc_blocked_wcs_wtfini_bad_cr)
TAB_BG_TRC_REC("  WcBlocked bt_get      ", wc_blocked_bt_get)
TAB_BG_TRC_REC("  WcBlocked wcs_cdb_sc  ", wc_blocked_wcs_cdb_sc_final_retry)
TAB_BG_TRC_REC("  wcb_bg_update_lckfail1", wcb_bg_update_lckfail1)
TAB_BG_TRC_REC("  wcb_bg_update_lckfail2", wcb_bg_update_lckfail2)
TAB_BG_TRC_REC("  wcb_wtstart_lckfail1  ", wcb_wtstart_lckfail1)
TAB_BG_TRC_REC("  wcb_wtstart_lckfail2  ", wcb_wtstart_lckfail2)
TAB_BG_TRC_REC("  wcb_wtstart_lckfail3  ", wcb_wtstart_lckfail3)
TAB_BG_TRC_REC("  wcb_wtstart_lckfail4  ", wcb_wtstart_lckfail4)
TAB_BG_TRC_REC("  wcb_wtfini_lckfail1   ", wcb_wtfini_lckfail1)
TAB_BG_TRC_REC("  wcb_wtfini_lckfail2   ", wcb_wtfini_lckfail2)
TAB_BG_TRC_REC("  wcb_wtfini_lckfail3   ", wcb_wtfini_lckfail3)
TAB_BG_TRC_REC("  wcb_wtfini_lckfail4   ", wcb_wtfini_lckfail4)
TAB_BG_TRC_REC("  WcBlocked dirtystuck1 ", wcb_t_end_sysops_dirtystuck1)
TAB_BG_TRC_REC("  WcBlocked dirtystuck2 ", wcb_t_end_sysops_dirtystuck2)
TAB_BG_TRC_REC("  WcBlocked secshrclnup2", wcb_secshr_db_clnup_wbuf_dqd)
