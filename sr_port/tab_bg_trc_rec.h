/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* All additions to the end please */

#if defined(VMS)
TAB_BG_TRC_REC("  Removed from free     ", rmv_free)
TAB_BG_TRC_REC("  Scratched clean       ", rmv_clean)
TAB_BG_TRC_REC("  Reused clean          ", clean_to_mod)
TAB_BG_TRC_REC("  Reused qio            ", qio_to_mod)
TAB_BG_TRC_REC("  Blocked by qio        ", blocked)
TAB_BG_TRC_REC("  Blkd qio fnshd 2 late ", blkd_made_empty)
TAB_BG_TRC_REC("  Obsolete to empty     ", obsolete_to_empty)
TAB_BG_TRC_REC("  Qio to clean          ", qio_to_clean)
TAB_BG_TRC_REC("  Former Activelvltriggr", active_lvl_trigger_obsolete)
TAB_BG_TRC_REC("  Former T_end new got b", new_buff_obsolete)
TAB_BG_TRC_REC("  Getnew buff           ", get_new_buff)
TAB_BG_TRC_REC("  Reused modified       ", mod_to_mod)
TAB_BG_TRC_REC("  Wcs_wtfini invoked    ", wcs_wtfini_invoked)
#elif defined(UNIX)
TAB_BG_TRC_REC("  Total buffer flushes  ", total_buffer_flush)		/* # of wcs_flu calls */
TAB_BG_TRC_REC("  Flsh for buff cnt     ", bufct_buffer_flush)		/* # of flushing-till-buffers-free-cnt (wcs_get_space) */
TAB_BG_TRC_REC("  Flsh for buff cnt lps ", bufct_buffer_flush_loop)	/* # of flushing-till-buffers-free-cnt looping back */
TAB_BG_TRC_REC("  Calls to wcs_wtstart  ", wrt_calls)			/* # of calls to wcs_wtstart */
TAB_BG_TRC_REC("  Writes by wcs_wtstart ", wrt_count)			/* # of writes done in wcs_wtstart */
TAB_BG_TRC_REC("  Writes were blocked   ", wrt_blocked)			/* # of times wc_blocked was on in wcs_wtstart */
TAB_BG_TRC_REC("  Writer was busy       ", wrt_busy)			/* Encountered wcs_wtstart lock */
TAB_BG_TRC_REC("  Writer fnd no writes  ", wrt_noblks_wrtn)		/* Times wcs_wtstart ran queues but nothing written */
TAB_BG_TRC_REC("  Reserved filler bg_trc", reserved_bgtrcrec1)		/* Reserved filler to match length of VMS section */
TAB_BG_TRC_REC("  Reserved filler bg_trc", reserved_bgtrcrec2)		/* Reserved filler to match length of VMS section */
TAB_BG_TRC_REC("  Reserved filler bg_trc", reserved_bgtrcrec3)		/* Reserved filler to match length of VMS section */
TAB_BG_TRC_REC("  Lost block recovery   ", lost_block_recovery)		/* Performing lost block recovery in gds_rundown  */
TAB_BG_TRC_REC("  WcBlocked onln_rlbk   ", wc_blocked_onln_rlbk)	/* Set by online rollback due to incomplete wcs_flu */
#else
# error Unsupported platform
#endif
TAB_BG_TRC_REC("  Stale                 ", stale)
TAB_BG_TRC_REC("  Starved               ", starved)
TAB_BG_TRC_REC("  Stale timer started   ", stale_timer_started)		/* Stale buffer timer started */
TAB_BG_TRC_REC("  Stale timer pop       ", stale_timer_pop)		/* Stale timer has popped */
TAB_BG_TRC_REC("  Stale process defer   ", stale_process_defer)		/* Deferring processing due to conditions */
TAB_BG_TRC_REC("  Stale deferd procsd   ", stale_defer_processed)	/* Stale processing done outside crit */
TAB_BG_TRC_REC("  Spcfc buff flshs      ", spcfc_buffer_flush)		/* Count of flushing specific buffer (wcs_get_space) */
TAB_BG_TRC_REC("  Spcfc buff flsh lps   ", spcfc_buffer_flush_loop)	/* # of passes to flush a specific buffer (wcs_get_space) */
TAB_BG_TRC_REC("  Spcfc buff flsh rtries", spcfc_buffer_flush_retries)	/* Times we re-flushed when 1st flush didn't flush buffer */
TAB_BG_TRC_REC("  Spcfc buff lkwait flsh", spcfc_buffer_flushed_during_lockwait)
TAB_BG_TRC_REC("  TP crit retries       ", tp_crit_retries)		/* # of times re-tried getting crit (common Unix & VMS) */
TAB_BG_TRC_REC("  DbCshGetn_FlushDirty  ", db_csh_getn_flush_dirty)	/* all the fields from now on use the BG_TRACE_PRO macro  */
TAB_BG_TRC_REC("  DbCshGetn_RipWait     ", db_csh_getn_rip_wait)	/* since they are incremented rarely, they will go in	  */
TAB_BG_TRC_REC("  DbCshGetn_BfOwnerStuck", db_csh_getn_buf_owner_stuck)	/* production code too. The BG_TRACE_PRO macro does a     */
TAB_BG_TRC_REC("  DbCshGetn_OutOfDesign ", db_csh_getn_out_of_design)	/* NON-INTERLOCKED increment.				  */
TAB_BG_TRC_REC("  TQread_BfOwnerStuck   ", t_qread_buf_owner_stuck)
TAB_BG_TRC_REC("  TQread_OutOfDesign    ", t_qread_out_of_design)
TAB_BG_TRC_REC("  BtPut_FlushDirty      ", bt_put_flush_dirty)
TAB_BG_TRC_REC("  M-lock wakeups        ", mlock_wakeups)		/* # of times a process has been awakened after lock wait */
TAB_BG_TRC_REC("  WcBlocked WcsReoverInv", wc_blocked_wcs_recover_invoked)
TAB_BG_TRC_REC("  WcBlocked WcsVerifyPas", wc_blocked_wcs_verify_passed)
TAB_BG_TRC_REC("  WcBlocked TQread Getn ", wc_blocked_t_qread_db_csh_getn_invalid_blk)
TAB_BG_TRC_REC("  WcBlocked TQread Get  ", wc_blocked_t_qread_db_csh_get_invalid_blk)
TAB_BG_TRC_REC("  WcBlocked Getn LoopXcd", wc_blocked_db_csh_getn_loopexceed)
TAB_BG_TRC_REC("  WcBlocked StarveWrite ", wc_blocked_db_csh_getn_wcsstarvewrt)
TAB_BG_TRC_REC("  WcBlocked DbCshGet    ", wc_blocked_db_csh_get)
TAB_BG_TRC_REC("  WcBlocked tptend Space", wc_blocked_tp_tend_wcsgetspace)
TAB_BG_TRC_REC("  WcBlocked tptend t1   ", wc_blocked_tp_tend_t1)
TAB_BG_TRC_REC("  WcBlocked tp bitmap   ", wc_blocked_tp_tend_bitmap)
TAB_BG_TRC_REC("  WcBlocked tp jnl cwset", wc_blocked_tp_tend_jnl_cwset)
TAB_BG_TRC_REC("  WcBlocked tp jnl wcflu", wc_blocked_tp_tend_jnl_wcsflu)
TAB_BG_TRC_REC("  WcBlocked tend hist   ", wc_blocked_t_end_hist)
TAB_BG_TRC_REC("  WcBlocked hist1 nul bt", wc_blocked_t_end_hist1_nullbt)
TAB_BG_TRC_REC("  WcBlocked hist1 no nul", wc_blocked_t_end_hist1_nonnullbt)
TAB_BG_TRC_REC("  WcBlocked bitmp nul bt", wc_blocked_t_end_bitmap_nullbt)
TAB_BG_TRC_REC("  WcBlocked bitmp no nul", wc_blocked_t_end_bitmap_nonnullbt)
TAB_BG_TRC_REC("  WcBlocked jnl cwset   ", wc_blocked_t_end_jnl_cwset)
TAB_BG_TRC_REC("  WcBlocked jnl wcflu   ", wc_blocked_t_end_jnl_wcsflu)
TAB_BG_TRC_REC("  DbCshGet TooManyLoops ", db_csh_get_too_many_loops)
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
TAB_BG_TRC_REC("  WcBlocked mutexsalvage", wcb_mutex_salvage)
TAB_BG_TRC_REC("  WcBlocked tp_grab_crit", wcb_tp_grab_crit)		/* currently unused */
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
TAB_BG_TRC_REC("  Dwngrd refmts syncIO  ", dwngrd_refmts_syncio)
TAB_BG_TRC_REC("  Dwngrd refmts asyncIO ", dwngrd_refmts_asyncio)
TAB_BG_TRC_REC("  Shmpool refmt harvests", shmpool_refmt_harvests)
TAB_BG_TRC_REC("  Shmpool recovery      ", shmpool_recovery)
TAB_BG_TRC_REC("  Shmpool blkd by sdc   ", shmpool_blkd_by_sdc)
TAB_BG_TRC_REC("  Shmpool alloc bbflush ", shmpool_alloc_bbflush)
TAB_BG_TRC_REC("  RefmtHvst blkrel repld", refmt_hvst_blk_released_replaced)
TAB_BG_TRC_REC("  RefmtHvst blkrel iocom", refmt_hvst_blk_released_io_complete)
TAB_BG_TRC_REC("  RefmtHvst blk kept    ", refmt_hvst_blk_kept)
TAB_BG_TRC_REC("  RefmtHvst blk ignrd   ", refmt_hvst_blk_ignored)
TAB_BG_TRC_REC("  RefmtBchk blk freed   ", refmt_blk_chk_blk_freed)
TAB_BG_TRC_REC("  RefmtBchk blk kept    ", refmt_blk_chk_blk_kept)
TAB_BG_TRC_REC("  Active level tripped  ", active_lvl_trigger)
TAB_BG_TRC_REC("  T_end new got buff    ", new_buff)
TAB_BG_TRC_REC("  CommitWait SleepInCrit", phase2_commit_wait_sleep_in_crit)
TAB_BG_TRC_REC("  CommitWait SleepNoCrit", phase2_commit_wait_sleep_no_crit)
TAB_BG_TRC_REC("  CommitWait PidCnt     ", phase2_commit_wait_pidcnt)
TAB_BG_TRC_REC("  WcBlocked intend_wait ", wcb_t_end_sysops_intend_wait)
TAB_BG_TRC_REC("  WcBlocked secshrclnup3", wcb_secshr_db_clnup_phase2_clnup)
TAB_BG_TRC_REC("  WcBlocked commitwait  ", wcb_phase2_commit_wait)
TAB_BG_TRC_REC("  recompute_upd_array # ", recompute_upd_array_calls)
TAB_BG_TRC_REC("  recompute_upd rip     ", recompute_upd_array_rip)
TAB_BG_TRC_REC("  recompute_upd in_tend ", recompute_upd_array_in_tend)
TAB_BG_TRC_REC("  recompute_upd srchblk ", recompute_upd_array_search_blk)
TAB_BG_TRC_REC("  recompute_upd new_rec ", recompute_upd_array_new_rec)
TAB_BG_TRC_REC("  recompute_upd rec_size", recompute_upd_array_rec_size)
TAB_BG_TRC_REC("  recompute_upd rec_cmpc", recompute_upd_array_rec_cmpc)
TAB_BG_TRC_REC("  recompute_upd blk_fini", recompute_upd_array_blk_fini)
TAB_BG_TRC_REC("  recompute_upd blksplit", recompute_upd_array_blk_split)
TAB_BG_TRC_REC("  T_qread ripsleep_cnt  ", t_qread_ripsleep_cnt)
TAB_BG_TRC_REC("  T_qread ripsleep_nblks", t_qread_ripsleep_nblks)
