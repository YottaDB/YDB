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

/* no additions/deletions to this file are allowed. each TAB_BG_TRC_REC entry corresponds to a field in the file-header */

#if defined(VMS)
TAB_BG_TRC_REC("  Removed from free     ", rmv_free)
TAB_BG_TRC_REC("  Scratched clean       ", rmv_clean)
TAB_BG_TRC_REC("  Reused clean          ", clean_to_mod)
TAB_BG_TRC_REC("  Reused qio            ", qio_to_mod)
TAB_BG_TRC_REC("  Blocked by qio        ", blocked)
TAB_BG_TRC_REC("  Blkd qio fnshd 2 late ", blkd_made_empty)
TAB_BG_TRC_REC("  Obsolete to empty     ", obsolete_to_empty)
TAB_BG_TRC_REC("  Qio to clean          ", qio_to_clean)
TAB_BG_TRC_REC("  Stale                 ", stale)
TAB_BG_TRC_REC("  Starved               ", starved)
TAB_BG_TRC_REC("  Active level tripped  ", active_lvl_trigger)
TAB_BG_TRC_REC("  T_end new got buff    ", new_buff)
TAB_BG_TRC_REC("  Getnew buff           ", get_new_buff)
TAB_BG_TRC_REC("  Reused modified       ", mod_to_mod)
#elif defined(UNIX)
TAB_BG_TRC_REC("  Total buffer flushes  ", total_buffer_flush)		/* Count of wcs_flu calls */
TAB_BG_TRC_REC("  Flsh for buff cnt     ", bufct_buffer_flush)		/* Count of flushing-till-buffers-free-cnt (wcs_get_space) */
TAB_BG_TRC_REC("  Flsh for buff cnt lps ", bufct_buffer_flush_loop)	/* Count of flushing-till-buffers-free-cnt looping back (wcs_get_space) */
TAB_BG_TRC_REC("  Stale timer started   ", stale_timer_started)		/* Stale buffer timer started */
TAB_BG_TRC_REC("  Stale timer pop       ", stale_timer_pop)		/* Stale timer has popped */
TAB_BG_TRC_REC("  Stale process defer   ", stale_process_defer)		/* Deferring processing due to conditions */
TAB_BG_TRC_REC("  Stale deferd procsd   ", stale_defer_processed)	/* Stale processing done outside crit */
TAB_BG_TRC_REC("  Calls to wcs_wtstart  ", wrt_calls)			/* Calls to wcs_wtstart */
TAB_BG_TRC_REC("  Writes by wcs_wtstart ", wrt_count)			/* Count of writes done in wcs_wtstart */
TAB_BG_TRC_REC("  Writes were blocked   ", wrt_blocked)			/* wc_blocked was on in wcs_wtstart */
TAB_BG_TRC_REC("  Writer was busy       ", wrt_busy)			/* Encountered wcs_wtstart lock */
TAB_BG_TRC_REC("  Writer fnd no writes  ", wrt_noblks_wrtn)		/* Times wcs_wtstart ran queues but nothing written */
TAB_BG_TRC_REC("  Reserved filler bg_trc", reserved_bgtrcrec)		/* Reserved filler to match length of VMS section */
TAB_BG_TRC_REC("  Lost block recovery   ", lost_block_recovery)		/* Performing lost block recovery in gds_rundown (traced PRO also) */
#else
# error Unsupported platform
#endif
TAB_BG_TRC_REC("  Spcfc buff flshs      ", spcfc_buffer_flush)		/* Count of flushing specific buffer (wcs_get_space) */
TAB_BG_TRC_REC("  Spcfc buff flsh lps   ", spcfc_buffer_flush_loop)	/* Passes through the active queue made to flush a specific buffer (wcs_get_space) */
TAB_BG_TRC_REC("  Spcfc buff flsh rtries", spcfc_buffer_flush_retries)	/* Times we re-flushed when 1st flush didn't flush buffer */
TAB_BG_TRC_REC("  Spcfc buff lkwait flsh", spcfc_buffer_flushed_during_lockwait)
TAB_BG_TRC_REC("  TP crit retries       ", tp_crit_retries)		/* Number of times we re-tried getting crit (common Unix & VMS) */
TAB_BG_TRC_REC("  DbCshGetn_FlushDirty  ", db_csh_getn_flush_dirty)	/* all the fields from now on use the BG_TRACE_PRO macro   */
TAB_BG_TRC_REC("  DbCshGetn_RipWait     ", db_csh_getn_rip_wait)	/* since they are incremented rarely, they will go in	   */
TAB_BG_TRC_REC("  DbCshGetn_BfOwnerStuck", db_csh_getn_buf_owner_stuck)	/* production code too. The BG_TRACE_PRO macro does a      */
TAB_BG_TRC_REC("  DbCshGetn_OutOfDesign ", db_csh_getn_out_of_design)	/* NON-INTERLOCKED increment.				   */
TAB_BG_TRC_REC("  TQread_BfOwnerStuck   ", t_qread_buf_owner_stuck)
TAB_BG_TRC_REC("  TQread_OutOfDesign    ", t_qread_out_of_design)
TAB_BG_TRC_REC("  BtPut_FlushDirty      ", bt_put_flush_dirty)
TAB_BG_TRC_REC("  M-lock wakeups        ", mlock_wakeups)		/* Times a process has slept on a lock in this region and been awakened */
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
