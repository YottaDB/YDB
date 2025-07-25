/****************************************************************
 *								*
 * Copyright (c) 2010-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note since this table is included multiple times, it should not have "previously included" protection.
 *
 * When adding items to this table, please be aware of alignment issues.
 *
 * Notes on where to put added items:
 *
 *   1. Make organization defensible. Possible organizations:
 *      a. Group like-types alphabetically.
 *      b. Group related items (such as compiler, replication, etc.
 *   2. Because references will use an offset into this structure and since the "immediate offset" in
 *      compiler generated instructions is usually limited to "smaller" values like less than 16K or
 *      32K or whatever (platform dependent), items near the top of the table should be reserved used for
 *      the most commonly used items (e.g. cs_addrs, cs_data, gv_target, etc).
 *   3. Larger arrays/structures should go nearer the end.
 *   4. There are no other runtime dependencies on this order. The order of fields can be switched around
 *      any way desired with just a rebuild.
 *   5. It is important for ANY DEBUG_ONLY fields to go at the VERY END. Failure to do this breaks gtmpcat.
 *   6. If a DEBUG_ONLY array is declared whose dimension is a macro, then it is necessary, for gtmpcat to work,
 *      that the macro shouldn't be defined as DEBUG_ONLY.
 */

/* Priority access fields - commonly used fields in performance situations */
THREADGBLDEF(grabbing_crit, 			gd_region *)			/* Region currently grabbing crit in (if any) */

/* Below line is kept at beginning of this file (instead of somewhere in the middle of the file) to avoid errors when
 * compiling sr_armv7l/dm_start.s since "ldr" instruction in ESTABLISH macro in sr_armv7l/error.si uses "#ggo_rts_error_depth"
 * and that has to be less than 4096 or else the assembler issues a "Error: bad immediate value for offset (4200)" error
 * (where 4200 offset is > 4096).
 */
THREADGBLDEF(rts_error_depth,			unsigned int)			/* Recursion level of rts_error_csa() */

/* Compiler */
THREADGBLDEF(blkmod_fail_level,			int4)				/* TP trace reporting element */
THREADGBLDEF(blkmod_fail_type,			int4)				/* TP trace reporting element */
THREADGBLDEF(block_level,			int4)				/* used to check embedded subroutine levels */
THREADGBLDEF(boolchain,				triple)				/* anchor for chain used by bx_boolop  */
THREADGBLDEF(boolchain_ptr,			triple *)			/* pointer to anchor for chain used by bx_boolop  */
THREADGBLDEF(bool_targ_anchor,			tbp)				/* anchor of targ chain for bool relocation */
THREADGBLDEF(bool_targ_ptr,			tbp *)				/* ptr->anchor of targ chain for bool relocation */
THREADGBLDEF(code_generated,			boolean_t)			/* flag that the compiler generated an object */
THREADGBLDEF(codegen_padlen,			int4)				/* used to pad code to section alignment */
THREADGBLDEF(compile_time,			boolean_t)			/* flag that the compiler's at work */
THREADGBLDEF(curtchain,				triple *)			/* pointer to anchor of current triple chain */
THREADGBLDEF(director_ident,			mstr)				/* look-ahead scanner mident from advancewindow */
THREADGBLDEF(director_mval,			mval)				/* look-ahead scanner mval from advancewindow*/
THREADGBLDEF(director_token,			char)				/* look-ahead scanner token from advancewindow */
THREADGBLDEF(dollar_zcstatus,			int4)				/* return status for zcompile and others */
THREADGBLDEF(expr_depth,			unsigned int)			/* expression nesting level */
THREADGBLDEF(expr_start,			triple *)			/* chain anchor for side effect early evaluation */
THREADGBLDEF(expr_start_orig,			triple *)			/* anchor used to test if there's anything hung on
										 * expr_start */
THREADGBLDEF(defined_symbols,			struct sym_table *)		/* Anchor for symbol chain */
THREADGBLDEF(for_stack_ptr,			oprtype **)			/* part of FOR compilation nesting mechanism */
THREADGBLDEF(ydb_fullbool,			unsigned int)			/* controls boolean side-effect behavior defaults
										 * to 0 (YDB_BOOL) */
THREADGBLDEF(ind_result,			mval *)				/* pointer to indirection return location */
THREADGBLDEF(ind_source,			mval *)				/* pointer to indirection source location */
THREADGBLDEF(indirection_mval,			mval)				/* used for parsing subscripted indirection */
THREADGBLDEF(last_source_column,		int)				/* parser tracker */
THREADGBLDEF(lexical_ptr,			char *)				/* parser character position */
THREADGBLDEF(linkage_first,			struct linkage_entry *)		/* Start of linkage (extern) list this routine */
THREADGBLDEF(linkage_last,			struct linkage_entry *)		/* Last added entry */
#ifdef USHBIN_SUPPORTED
THREADGBLDEF(objhash_state,			hash128_state_t)		/* Seed value - progressive hash of object file */
#endif
THREADGBLDEF(discard,				boolean_t)                      /* parsing after literal FALSE postconditional */
THREADGBLDEF(pos_in_chain,			triple)				/* anchor used to restart after a parsing error */
THREADGBLDEF(rts_error_in_parse,		boolean_t)			/* flag to stop parsing current line */
THREADGBLDEF(s2n_intlit, 			boolean_t)			/* type info from s2n for advancewindow */
THREADGBLDEF(routine_source_offset,		uint4)				/* offset of M source within literal text pool */
THREADGBLDEF(saw_side_effect,			boolean_t)			/* need side effect handling other than naked */
THREADGBLDEF(shift_side_effects, 		int)				/* flag shifting of side-effects ahead of boolean
										 * evaluation */
THREADGBLDEF(side_effect_base,			boolean_t *)			/* anchor side effect array: bin ops & func args */
THREADGBLDEF(side_effect_depth,			uint4)				/* current high water of side effect expr array */
THREADGBLDEF(side_effect_handling,		int)				/* side effect handling in actuallists, function
										 * args & non-boolean binary operator operands */
THREADGBLDEF(source_buffer,			mstr)				/* source line buffer control */
THREADGBLDEF(source_error_found,		int4)				/* flag to partially defer compiler error */
THREADGBLDEF(temp_subs,				boolean_t)			/* flag temp storing of subscripts to preserve
										 * current evaluation */
THREADGBLDEF(trigger_compile_and_link,		boolean_t)			/* A trigger compilation/link is active */
THREADGBLDEF(window_ident,			mstr)				/* current scanner mident from advancewindow */
THREADGBLDEF(window_mval,			mval)				/* current scanner mval from advancewindow */
THREADGBLDEF(window_token,			unsigned char)			/* current scanner token from advancewindow */
THREADGBLDEF(xecute_literal_parse,		boolean_t)			/* flag TRUE when trying what its name says */
THREADGBLDEF(fetch_control,			fetch_ctrl)			/* structure for managing lvn fetches */

/* Database */
THREADGBLDEF(dbinit_max_delta_secs,		uint4)				/* max time before we bail out in db_init */
THREADGBLDEF(dollar_zmaxtptime, 		int4)				/* tp timeout in seconds */
THREADGBLAR1DEF(save_xfer_root,		save_xfer_entry, DEFERRED_EVENTS)	/* array of deferred events with the zeroth
										 * acting as the root of a queue identified by the
										 * _ptr item immediately following
										 */
THREADGBLDEF(save_xfer_root_ptr,		save_xfer_entry*) 		/* pointer to anchor; see immediately prior item */
THREADGBLDEF(ztimeout_timer_on,		boolean_t)			/* ztimeout has a gt_timer pending */
THREADGBLDEF(dollar_ztimeout,			dollar_ztimeout_struct)		/* holds ztimeout state info */
THREADGBLDEF(donot_write_inctn_in_wcs_recover,	boolean_t)			/* TRUE if wcs_recover should NOT write INCTN */
THREADGBLDEF(gbuff_limit,			mval)				/* holds a GTM_POOLLIMIT value for REORG or DBG */
THREADGBLDEF(gd_targ_tn,			trans_num)			/* number that is incremented for every gvcst_spr*
										 * action. helps easily determine whether a region
										 * has already been seen in this gvcst_spr* action.
										 */
THREADGBLDEF(gd_targ_reg_array,			trans_num *)			/* Indicates which regions are already part of
										 * current spanning-region database action.
										 * Array is NULL if no spanning globals were
										 * detected as part of opening global directory.
										 */
THREADGBLDEF(gd_targ_reg_array_size,		uint4)				/* Size of current gd_targ_reg_array allocation.
										 * Non-zero only if at least one spanning global
										 * has been seen at time of gld open.
										 * Note: the dimension is the # of regions involved
										 * and not the # of bytes allocated in the array.
										 */
THREADGBLDEF(gd_targ_addr,			gd_addr *)			/* current global directory reference. Needed by
										 * name level $order or $zprevious to know inside
										 * op_gvorder.c/op_zprevious.c whether the global
										 * reference went through op_gvname/op_gvextnam.
										 * Also needed by gvcst_spr_*.c etc.
										 */
THREADGBLDEF(gd_targ_gvnh_reg,			gvnh_reg_t *)			/* Pointer to the gvnh_reg corresponding to the
										 * unsubscripted gvn reference that last went
										 * through op_gvname/op_gvnaked/op_gvextnam.
										 * Set to a non-NULL value for spanning globals
										 * and to NULL for non-spanning globals (because
										 * it offers an easy way to check for spanning
										 * globals in op_gv* functions before invoking
										 * the corresponding gvcst_spr_* functions).
										 * Needed by op_gvorder etc. to avoid having to
										 * recompute this based on gd_header & gv_currkey.
										 */
THREADGBLDEF(gd_targ_map,			gd_binding *)			/* map entry to which "gv_bind_subsname" bound
										 * the subscripted gvn. Maintained ONLY for spanning
										 * globals. Cannot be relied upon in case the
										 * current reference is for a non-spanning global.
										 * (i.e. is usable only if gd_targ_gvnh_reg->gvspan
										 * is non-NULL). This is needed by gvcst_spr_*
										 * functions to avoid recomputing the map (using
										 * gv_srch_map) based on gd_header/gv_currkey.
										 */
THREADGBLDEF(ydb_custom_errors,			mstr)
THREADGBLDEF(gv_extname_size,			int4)				/* part op_gvextname working memory mechanism */
THREADGBLDEF(gv_last_subsc_null,		boolean_t)			/* indicates whether the last subscript of
										 * gv_currkey (aka $reference) is a NULL string */
THREADGBLDEF(gv_mergekey2,			gv_key *)			/* op_merge working memory */
THREADGBLDEF(gv_reorgkey,			gv_key *)			/* mu_swap_blk working memory */
THREADGBLDEF(gv_some_subsc_null,		boolean_t)			/* TRUE if SOME subscript other than the last is
										 * NULL in gv_currkey (aka $REFERENCE). Note that
										 * while "some" in var name might typically include
										 * the last subscript, it does NOT in this case and
										 * allows name to be kept shorter. */
THREADGBLDEF(gv_sparekey,			gv_key *)			/* gv_xform_key working memory */
THREADGBLDEF(gv_sparekey_mval,			mval)				/* gv_xform_key working memory */
THREADGBLDEF(gv_sparekey_size,			int4)				/* part gv_xform_key working memory mechanism */
THREADGBLDEF(gv_tporigkey_ptr,			gv_key_buf *)			/* copy of gv_currkey at outermost TSTART */
THREADGBLDEF(gv_tporig_extnam_str,		mstr)				/* copy of extnam_str at outermost TSTART */
THREADGBLDEF(in_gvcst_bmp_mark_free,		boolean_t)			/* May need to skip online rollback cleanup or
										 * gvcst_redo_root_search on a restart */
THREADGBLDEF(in_gvcst_redo_root_search,		boolean_t)			/* TRUE if gvcst_redo_root_search is in C-stack */
THREADGBLDEF(in_op_gvget,			boolean_t)			/* TRUE if op_gvget() is a C-stack call ancestor */
THREADGBLDEF(issue_DBROLLEDBACK_anyways,	boolean_t)			/* currently set by MUPIP LOAD */
THREADGBLDEF(last_fnquery_return_subcnt,	int)				/* count subscript in last_fnquery_return_sub */
THREADGBLDEF(last_fnquery_return_varname,	mval)				/* returned varname of last $QUERY() */
THREADGBLDEF(nontprestart_count,		uint4)				/* non-tp restart counter */
THREADGBLDEF(nontprestart_log_first,		int4)				/* # of non-tp restarts logged unconditionally */
THREADGBLDEF(nontprestart_log_delta,		int4)				/* defines every n-th restart to be logged for
										 * non-tp */
THREADGBLDEF(block_now_locked,			cache_rec_ptr_t)		/* db_csh_getn sets for secshr_sb_clnup exit */
THREADGBLDEF(ok_to_call_wcs_recover,		boolean_t)			/* Set to TRUE before a few wcs_recover callers.
										 * Any call to wcs_recover in the final retry
										 * assert to prevent cache recovery while in a
										 * transaction and confuse things enough to cause
										 * further restarts (which is out-of-design while
										 * in the final retry). */
THREADGBLDEF(prev_gv_target,			gv_namehead *)			/* saves the last gv_target for debugging */
THREADGBLDEF(ready2signal_gvundef,		boolean_t)			/* TRUE if GET operation about to signal GVUNDEF */
THREADGBLDEF(redo_rootsrch_ctxt,		redo_root_search_context)	/* context to be saved and restored during
										 * gvcst_redo_root_search */
THREADGBLDEF(skip_DB_exists_check,		boolean_t)			/* skip check for whether the DB file exists */
THREADGBLDEF(skip_file_corrupt_check,		boolean_t)			/* skip file_corrupt check in grab_crit */
THREADGBLDEF(tpnotacidtime,			mval)				/* limit for long non-ACID ops in transactions */
THREADGBLDEF(tp_restart_count,			uint4)				/* tp_restart counter */
THREADGBLDEF(tp_restart_dont_counts,		int4)				/* tp_restart count adjustment; NOTE: DEBUG only */
THREADGBLDEF(tp_restart_entryref,		mval)				/* tp_restart position for reporting */
THREADGBLDEF(tp_restart_failhist_indx,		int4)				/* tp_restart dbg restart history index */
THREADGBLDEF(tprestart_syslog_delta,		int4)				/* defines every n-th restart to be logged for tp */
THREADGBLDEF(tprestart_syslog_first,		int4)				/* # of TP restarts logged unconditionally */
THREADGBLAR1DEF(t_fail_hist_blk,		block_id,	(CDB_MAX_TRIES))/* array for TP tracing */
THREADGBLAR1DEF(tp_fail_bttn,			trans_num,	(CDB_MAX_TRIES))/* array for TP tracing */
THREADGBLAR1DEF(tp_fail_histtn,			trans_num,	(CDB_MAX_TRIES))/* array for TP tracing */
THREADGBLAR1DEF(tp_fail_hist,			gv_namehead *,	(CDB_MAX_TRIES))/* array for TP tracing */
THREADGBLAR1DEF(tp_fail_hist_reg,		gd_region *,	(CDB_MAX_TRIES))/* array for TP tracing */
THREADGBLDEF(wcs_recover_done,			boolean_t)			/* TRUE if wcs_recover was ever invoked in this
										 * process. */
THREADGBLDEF(statsdb_fnerr_reason,		int)				/* Failure code for "gvcst_set_statsdb_fname" */
THREADGBLDEF(statsdb_memerr,			boolean_t)			/* If true, Failed to write to statsdb (SIG7)" */
THREADGBLDEF(non_tp_noiso_key_n_value,		key_cum_value)			/* for non-TP recompute block */

/* Local variables */
THREADGBLDEF(curr_symval_cycle,			unsigned int)			/* When curr_symval is changed, counter is bumped */
THREADGBLDEF(in_op_fnnext,			boolean_t)			/* set TRUE by op_fnnext; FALSE by op_fnorder */
THREADGBLDEF(local_collseq,			collseq *)			/* pointer to collation algorithm for lvns */
THREADGBLDEF(local_collseq_stdnull,		boolean_t)			/* flag temp controlling empty-string subscript
										 * handling - if true, use standard null subscript
										 * collation for local variables */
THREADGBLDEF(local_coll_nums_as_strings,	boolean_t)			/* flag controlling whether local variables that
										 * evaluate to numbers are treated like numbers
										 * (collating before strings) or like strings in
										 * local collations */
THREADGBLDEF(lvmon_active,			boolean_t)			/* TRUE when local var monitoring is active */
THREADGBLDEF(lvmon_vars_anchor,			lvmon_var *)			/* Anchor for lv monitoring structure */
THREADGBLDEF(lvmon_vars_count,			int)				/* Count of lvmon_vars at lvmon_vars_anchor */
THREADGBLDEF(lv_null_subs,			int)				/* set in gtm_env_init_sp() */
THREADGBLDEF(max_lcl_coll_xform_bufsiz,		int)				/* max size of local collation buffer,which extends
										 * from 32K each time the buffer overflows */
/* Replication variables */
THREADGBLDEF(replgbl,				replgbl_t)			/* set of global variables needed by the source
										 * server */
THREADGBLDEF(tqread_nowait,			boolean_t)			/* avoid sleeping in t_qread if TRUE */
/* Auditing related global variables */
THREADGBLDEF(dollar_zaudit, 			boolean_t)			/* Intrinsic that indicates whether direct mode
										 * auditing (i.e. APD) is enabled.
										 * TRUE => Auditing is enabled. */
THREADGBLDEF(is_zauditlog, 			boolean_t)			/* Index for audit_conn*/
/* Miscellaneous */
THREADGBLDEF(arlink_enabled,			boolean_t)			/* TRUE if any zroutines segment is autorelink
										 * enabled. */
THREADGBLDEF(arlink_loaded,			uint4)				/* Count of auto-relink enabled routines linked */
THREADGBLDEF(collseq_list,			collseq *)			/* list of pointers to currently mapped collation
										 * algorithms - since this seems only used in
										 * collseq.c -seems more like a STATICDEF */
THREADGBLFPTR(create_fatal_error_zshow_dmp_fptr, void, 		(void))		/* Fptr for gtm_fatal_error* zshow dmp routine */
THREADGBLDEF(stapi_errstr,			ydb_buffer_t *)			/* Pointer to ydb_buffer_t that is filled in case
										 * of error inside a SimpleThreadAPI call.
										 */
THREADGBLDEF(disable_sigcont,			boolean_t)			/* indicates whether the SIGCONT signal
										 * is allowed internally */
THREADGBLDEF(dollar_zcompile,			mstr)				/* compiler qualifiers */
THREADGBLDEF(dollar_etrap,			mval)				/* $etrap - standard error action */
THREADGBLDEF(dollar_zmode,			mval)				/* run mode indicator */
THREADGBLDEF(dollar_zonlnrlbk,			int)				/* ISV (incremented for every online rollback) */
THREADGBLDEF(dollar_zclose,			int)				/* ISV (set to close status for PIPE device) */
THREADGBLDEF(dollar_zroutines,			mstr)				/* routine search list */
THREADGBLDEF(dollar_zstep,			mval)				/* $zstep - action repository for zstep */
THREADGBLDEF(zstep_action,			mval)				/* $zstep - actual action for zstep */
THREADGBLDEF(dollar_ztrap,			mval)				/* $ztrap - recursive try error action */
THREADGBLDEF(error_on_jnl_file_lost,		unsigned int)			/* controls error handling done by jnl_file_lost.
										 * 0 (default) : Turn off journaling and continue.
										 * 1 : Keep journaling on, throw rts_error */
THREADGBLDEF(fnzsearch_lv_vars,			lv_val *)			/* op_fnzsearch lv tree anchor */
THREADGBLDEF(fnzsearch_sub_mval,		mval)				/* op_fnzsearch subscript constuctor */
THREADGBLDEF(fnzsearch_nullsubs_sav,		int)				/* op_fnzsearch temp for null subs control */
THREADGBLDEF(fnzsearch_globbuf_ptr,		glob_t *)			/* op_fnzsearch temp for pointing to glob results */
THREADGBLDEF(glvn_pool_ptr,			glvn_pool *)			/* Pointer to the glvn pool */
THREADGBLDEF(zhalt_retval,			int)				/* Used by "ydb_cij()" and "ydb_ci_exec()" to note
										 * down ZHALT return value.
										 */
#ifdef GTMDBGFLAGS_ENABLED
THREADGBLDEF(ydb_dbgflags,			int)
THREADGBLDEF(ydb_dbgflags_freq,			int)
THREADGBLDEF(ydb_dbgflags_freq_cntr,		int)
#endif
THREADGBLDEF(gtm_env_init_started,		boolean_t)			/* gtm_env_init flag envvar processing */
THREADGBLFPTR(gtm_env_xlate_entry,		int,		())		/* gtm_env_xlate() or ydb_env_xlate() function pointer */
THREADGBLDEF(ydb_environment_init,		boolean_t)			/* indicates a development environment rather
										 * than a production environment */
THREADGBLFPTR(gtm_sigusr1_handler,		void, 		(void))		/* SIGUSR1 signal handler function ptr */
THREADGBLDEF(ydb_linktmpdir,			mstr)				/* Directory to use for relinkctl files */
THREADGBLDEF(gtm_tmpdir,			mstr)				/* Directory to use for tmp files */
THREADGBLDEF(ydb_trigger_etrap,			mval)				/* $etrap - for use in triggers */
THREADGBLDEF(gtm_trctbl_cur,			trctbl_entry *)			/* Current gtm trace table entry */
THREADGBLDEF(gtm_trctbl_end,			trctbl_entry *)			/* End of gtm trace table (last entry + 1) */
THREADGBLDEF(gtm_trctbl_groups,			unsigned int)			/* Trace group mask (max 31 groups) */
THREADGBLDEF(gtm_trctbl_start,			trctbl_entry *)			/* Start of gtm trace table */
THREADGBLDEF(gtm_waitstuck_script,		mstr)				/* Path to the script to be executed during waits*/
THREADGBLDEF(gtmprompt,				mstr)				/* mstr pointing to prombuf containing the GTM
										 * prompt */
THREADGBLDEF(gtmsecshr_comkey,			unsigned int)			/* Hashed version key for gtmsecshr communications
										 * eliminates cross-version issues */
THREADGBLDEF(gvcst_statsDB_open_ch_active,	boolean_t)			/* Condition handler is active */
THREADGBLDEF(in_zwrite,				boolean_t)			/* ZWrite is active */
THREADGBLDEF(is_socketpool,			boolean_t)			/* True when device-to-be-opened is socketpool */
THREADGBLDEF(ydb_socket_keepalive_idle,		int)				/* Initialized from $ydb_socket_keepalive_idle */
THREADGBLDEF(in_mupip_integ,			boolean_t)			/* To let DO_DB_HDR_CHECK skip DBFLCORRP */
THREADGBLDEF(instance_frozen_crit_skipped,	boolean_t)			/* To indicate Instance Freeze is on, CRIT skipped*/
THREADGBLDEF(integ_cannotskip_crit,		boolean_t)			/* indicates whether a SKIP CRIT is allowed */
THREADGBLDEF(retry_buffer_returned_null,	boolean_t)			/* If a retried block in integ returned a NULL */
THREADGBLDEF(lab_lnr,				lnr_tabent **)			/* Passes address from op_rhd_ext to op_extcall etc.
										 * Points into either lab_proxy or linkage table
										 */
THREADGBLDEF(jobexam_counter,			unsigned int)			/* How many times invoked $ZJOBEXAM() this proc */
#ifdef AUTORELINK_SUPPORTED
THREADGBLDEF(lnk_proxy,				lnk_tabent_proxy)		/* Proxy linkage table for rtnhdr/labtab args for
										 * indirect calls */
#else
THREADGBLDEF(lab_proxy,				lab_tabent_proxy)		/* Placeholder storing lab_ln_ptr offset / lnr_adr
										 * pointer and has_parms value, so they are
										 * contiguous in memory */
#endif
THREADGBLDEF(libyottadb_active_rtn,		libyottadb_routines)		/* Which routine is currently active */
THREADGBLDEF(mprof_alloc_reclaim,		boolean_t)			/* Flag indicating whether the temporarily allocated
										 * memory should be reclaimed */
THREADGBLDEF(mprof_chunk_avail_size,		int)				/* Number of mprof stack frames that can fit in
										 * the current chunk */
THREADGBLDEF(mprof_env_gbl_name,		mval)				/* Name of global to use in mprof testing; should
										 * be undefined to not use explicit mprof testing,
										 * and empty or '0' if mprof data should not be
										 * dumped into a global at the end */
THREADGBLDEF(mprof_ptr,				mprof_wrapper *)		/* Object containing key mprof references */
THREADGBLDEF(mprof_reclaim_addr,		char *)				/* Address of the memory bucket before
										 * unw_prof_frame temporary allocations in case they
										 * go to new bucket */
THREADGBLDEF(mprof_reclaim_cnt,			int)				/* Amount of mem to reclaim after unw_prof_frame */
THREADGBLDEF(mprof_stack_curr_frame, 		mprof_stack_frame *)		/* Pointer to the last frame on the mprof stack */
THREADGBLDEF(mprof_stack_next_frame, 		mprof_stack_frame *)		/* Pointer to the next frame to be put on the
										 * mprof stack */
THREADGBLDEF(mu_cre_file_openrc,		int)				/* 0 if success otherwise holds errno after open */
#ifdef AUTORELINK_SUPPORTED
THREADGBLDEF(open_relinkctl_list,		open_relinkctl_sgm *)		/* Anchor for open relinkctl list; similar to
										 * open_shlib_root */
THREADGBLDEF(relinkctl_shm_min_index,		int)				/* Minimum size of rtnobj shared memory segment
										 * is 2**relinkctl_shm_min_index
										 */
THREADGBLDEF(ydb_autorelink_keeprtn,		boolean_t)			/* do not let go of objects in rtnobj shm */
#endif
THREADGBLDEF(open_shlib_root,			open_shlib *)			/* Anchor for open shared library list */
THREADGBLDEF(parm_pool_ptr,			parm_pool *)			/* Pointer to the parameter pool */
THREADGBLDEF(parms_cnt,                         unsigned int)                   /* Parameters count */
THREADGBLDEF(sapi_query_node_subs,		mstr *)				/* -> Array of YDB_MAX_SUBS mstrs holding subs
										 * .. to return to ydb_node_*_s(). */
THREADGBLDEF(sapi_query_node_subs_cnt,		int)				/* Count of subs filled in */
THREADGBLAR1DEF(zpeek_regname,			char,		NAME_ENTRY_SZ)	/* Last $ZPEEK() region specified */
THREADGBLDEF(zpeek_regname_len,			int)				/* Length of zpeekop_regname */
THREADGBLDEF(zpeek_reg_ptr,			gd_region *)			/* Resolved pointer for zpeekop_regname */
THREADGBLDEF(pipefifo_interrupt,		int)				/* count of number of times a pipe or fifo device is
										 * interrupted */
THREADGBLDEF(prof_fp,				mprof_stack_frame *)		/* Stack frame that mprof currently operates on */
THREADGBLDEF(relink_allowed,			int)				/* Non-zero if recursive relink permitted */
#ifdef AUTORELINK_SUPPORTED
THREADGBLDEF(save_zhist,			zro_hist *)			/* Temp storage for zro_hist blk so condition hndler
										 * can get a hold of it if necessary to free it */
#endif
THREADGBLAR1DEF(sapi_mstrs_for_gc_ary,		mstr *, MAX_SAPI_MSTR_GC_INDX + 1)	/* SimpleAPI mstr array needed by GC */
THREADGBLDEF(sapi_mstrs_for_gc_indx,		int)				/* Index into gc mstr array of next avail slot */
THREADGBLDEF(set_zroutines_cycle,		uint4)				/* Informs us if we changed $ZROUTINES between
										 * linking a routine and invoking it
										 */
THREADGBLDEF(statsDB_init_defer_anchor,		statsDB_deferred_init_que_elem *) /* Anchor point for deferred init of statsDBs */
THREADGBLDEF(statshare_opted_in,		uint4)				/* Flag controlling stats collection */
THREADGBLDEF(trans_code_pop,			mval)				/* trans_code holder for $ZTRAP popping */
THREADGBLDEF(view_ydirt_str,			char *)				/* op_view working storage for ydir* ops */
THREADGBLDEF(view_ydirt_str_len,		int4)				/* Part of op_view working storage for ydir* ops */
THREADGBLDEF(view_region_list,			tp_region *)			/* used by view_arg_convert and op_view/view_dbop */
THREADGBLDEF(view_region_free_list,		tp_region *)			/* used by view_arg_convert and op_view/view_dbop */
THREADGBLDEF(ydb_error_code,			int)				/* Error reflected back to condition handler - it
										 * is saved here so the ESTABLISHer has access */
THREADGBLFPTR(ydb_gbldir_xlate_entry,		int,		())		/* ydb_gbldir_xlate() function pointer */
THREADGBLDEF(zdate_form,			int4)				/* Control for default $zdate() format */
THREADGBLAR1DEF(zintcmd_active,			zintcmd_active_info,	ZINTCMD_LAST)	/* Interrupted timed commands */
THREADGBLDEF(zro_root,				zro_ent *)			/* Anchor for zroutines structure entry array */
THREADGBLDEF(zsearch_var,			lv_val *)			/* UNIX $zsearch() lookup variable */
THREADGBLDEF(ztrap_form,			int4)				/* ztrap type indicator */
THREADGBLDEF(poll_fds_buffer,			char *)				/* Buffer for poll() argument */
THREADGBLDEF(poll_fds_buffer_size,		size_t)				/* Current allocated size of poll_fds_buffer */
THREADGBLDEF(socket_handle_counter,		int)				/* Counter for generated socket handles */
THREADGBLDEF(mu_set_file_noencryptable,		boolean_t)			/* Disabling encryption relaxes encr errors */
/* Larger structures and char strings */
THREADGBLAR1DEF(director_string,		char,	SIZEOF(mident_fixed)*2)	/* Buffer for director_ident */
THREADGBLDEF(fnpca,				fnpc_area)			/* $Piece cache structure area */
THREADGBLAR1DEF(for_stack,			oprtype *,	MAX_FOR_STACK)	/* Stacks FOR scope complete (compilation) addrs */
THREADGBLAR1DEF(for_temps,			boolean_t,	MAX_FOR_STACK)	/* Stacked flags of FOR control value temps */
THREADGBLDEF(ydb_utfcgr_strings,		int)				/* Strings we can keep UTF8 parsing cache for */
THREADGBLDEF(ydb_utfcgr_string_groups,		int)				/* Groups of chars we can keep for each string */
THREADGBLAR1DEF(last_fnquery_return_sub,	mval,		MAX_LVSUBSCRIPTS)/* Returned subscripts of last $QUERY() */
THREADGBLDEF(lcl_coll_xform_buff,		char *)				/* This buffer is for local collation
										 * transformations, which must not nest - i.e.
										 * a transformation routine must not call another,
										 * or itself. This kind of nesting would cause
										 * overwriting of the buffer */
THREADGBLDEF(protmem_ba,			mstr)				/* Protected buffer */
THREADGBLAR1DEF(parm_ary,                       char *,         MAX_PARMS)      /* Parameter strings buffer */
THREADGBLAR1DEF(parm_ary_len,                   int,            MAX_PARMS)      /* Array element allocation length */
THREADGBLAR1DEF(parm_str_len,                   int,            MAX_PARMS)      /* Parameter strings lengths */
THREADGBLAR1DEF(prombuf,			char,	(MAX_MIDENT_LEN + 1))	/* The prompt buffer size (32) would allow at
										 * least 8 UTF8 characters, but since most
										 * commonly used UTF8 characters only occupy up
										 * to 3 bytes, the buffer would at least
										 * accommodate 10 UTF8 characters in a prompt */
THREADGBLAR1DEF(tmp_object_file_name,		char,	YDB_PATH_MAX)		/* Hold temporary object name across routines */
THREADGBLAR1DEF(tp_restart_failhist_arry,	char,	FAIL_HIST_ARRAY_SIZE)	/* tp_restart dbg storage of restart history */
#ifdef UTF8_SUPPORTED
THREADGBLDEF(utfcgra,				utfcgr_area)			/* Lookaside cache for UTF8 parsing */
#endif
THREADGBLDEF(utfcgr_string_lookmax,		int)				/* How many times to look for unreferenced slot */
THREADGBLAR1DEF(window_string,			char,	SIZEOF(mident_fixed))	/* Buffer for window_ident */

/* Utility I/O */
THREADGBLDEF(last_va_list_ptr,			va_list)			/* Last variable-length argument list used for util
										 * out buffer management */
THREADGBLAR1DEF(util_outbuff,			char,	OUT_BUFF_SIZE * UTIL_OUTBUFF_STACK_SIZE) /* Util output buffer */
THREADGBLDEF(util_outbuff_ptr,			char *)				/* Pointer to util output buffer */
THREADGBLDEF(util_outptr,			char *)				/* Pointer within util output buffer */


/* YottaDB Call-in related globals */
THREADGBLDEF(ci_table_default,			ci_tab_entry_t *)		/* Call-in table corresponding to "ydb_ci" env var.
										 * Used as default call-in table by "ydb_ci()"
										 * if TREF(ci_table_curr) is NULL.
										 */
THREADGBLDEF(ci_table_internal_filter,		ci_tab_entry_t *)		/* Call-in table corresponding to Internal filter
										 * (i.e. called from "gtm_ci_filter".
										 */
THREADGBLDEF(ci_table_curr, 			ci_tab_entry_t *)		/* Call-in table corresponding to most recent
										 * "ydb_ci_tab_switch" call; if no such call
										 * happened, the call-in table corresponding to
										 * "ydb_ci" env var .
										 */
THREADGBLDEF(ci_table_all, 			ci_tab_entry_t *)		/* Linked list of ALL open call-in tables */
THREADGBLDEF(extcall_package_root,		struct extcall_package_list *)	/* External call table package list */
THREADGBLDEF(gtmci_nested_level,		unsigned int)			/* Current nested depth of callin environments */
THREADGBLDEF(comm_filter_init,			boolean_t)			/* Signifying that the filter is in use */
THREADGBLDEF(temp_fgncal_stack,			unsigned char *)		/* Override for fgncal_stack when non-NULL */
THREADGBLDEF(zwrite_output_hook,		void *)				/* Really a void (*)(void), called if not NULL
										 * when zwr_output is ready for use.
										 */
THREADGBLDEF(want_empty_gvts,			boolean_t)			/* set to TRUE by MUPIP REORG when it is selecting
										 * globals to be reorged. Need to be able to select
										 * killed globals for effective truncate. */
THREADGBLDEF(in_mu_swap_root_state,		unsigned int)			/* Three states:
										 * MUSWP_INCR_ROOT_CYCLE: MUPIP REORG moves GVT
										 *		    root blocks in mu_swap_root.
										 * MUSWP_FREE_BLK: MUPIP REORG frees blocks
										 * MUSWP_NONE
										 */
THREADGBLDEF(prev_t_tries,			unsigned int)			/* t_tries - 1, before t_retry/tp_restart */
THREADGBLDEF(rlbk_during_redo_root,		boolean_t)			/* set to TRUE if an online rollback which takes
										 * the db to a different logical state occurs
										 * during gvcst_redo_root_search in t_retry */
THREADGBLDEF(mlk_yield_pid,			uint4)				/* if non-zero, indicates the process id for which
										 * we yielded a chance to get this lock. We keep
										 * yielding as long as the process-id that is the
										 * first in the wait queue ("d->pending") changes.
										 * If the pid stays the same, we stop yielding.
										 * if -1, indicates we disabled fairness mechanism
										 * in order to avoid livelocks.
										 */
THREADGBLDEF(jnl_extract_nocol,			uint4)				/* If non-zero, MUPIP JOURNAL EXTRACT will skip
										 * reading the directory tree to find the
										 * collation information for each global. Note
										 * that this might result in garbled subscripts
										 * in the extract if globals do use alternative
										 * collation.
										 */
THREADGBLDEF(skip_gtm_putmsg,			boolean_t)			/* currently needed by GDE to avoid gtm_putmsg
										 * from happening inside map_sym.c/fgn_getinfo.c
										 * when a VIEW "YCHKCOLL" is done as otherwise this
										 * pollutes the current device GDE is writing to
										 * and causes a GDECHECK error. This variable might
										 * also be useful for others as the need arises.
										 */
THREADGBLDEF(spangbl_seen,			boolean_t)	/* The process has referenced at least one global which spans
								 * multiple regions. Used by op_gvnaked currently.
								 */
THREADGBLDEF(no_spangbls,			boolean_t)	/* This process does not need to worry about spanning regions.
								 * Examples are DSE which operates on a specific region
								 * irrespective of whether the global spans regions or not.
								 */
THREADGBLDEF(max_fid_index,			int)		/* maximum value of csa->fid_index across all open csa's */
THREADGBLDEF(is_mu_rndwn_rlnkctl,		int)		/* this process is MUPIP RUNDOWN -RELINKCTL */
THREADGBLDEF(expand_prev_key,			int)		/* Can hold one of 3 values.
								 * TRUE implies we are inside a $zprevious or reverse $query call.
								 *	It kicks in an optimization where "gvcst_search_blk" and
								 *	"gvcst_search_tail" will expand prev_key as they do the
								 *	search. This avoids a later call to "gvcst_expand_key" to
								 *	determine prev_key after the search.
								 * ZPREVIOUS_NULL_SUBS_LEVEL1 implies we are inside a
								 *	$zprevious call and that we are inside a $zprevious(gvn)
								 *	where gvn is of the form ^gblname("") i.e. there is only
								 *	one subscript and that is the null subscript. This kicks in
								 *	in a "gvcst_search" optimization in addition to the above
								 *	prev_key optimization for the TRUE value.
								 * FALSE implies we are not inside a $zprevious call. None of the
								 *	above two optimizations kick in.
								 */
THREADGBLDEF(ydb_autorelink_ctlmax,		uint4)		/* Maximum number of routines allowed for autorelink */
/* Each process that opens a database file with O_DIRECT (which happens if asyncio=TRUE) needs to do
 * writes from a buffer that is aligned at the filesystem-blocksize level. We ensure this in database shared
 * memory global buffers where each buffer is guaranteed aligned at OS_PAGE_SIZE as private memory buffers that are
 * used (e.g. dbfilop etc.). All the private memory usages will use the global variable "dio_buff.aligned".
 * It is guaranteed to be OS_PAGE_SIZE aligned. "dio_buff.unaligned_size" holds the size of the allocated buffer
 * and it has enough space to hold one GDS block (max db blocksize across ALL dbs opened by this process till now)
 * with OS_PAGE_SIZE padding. "dio_buff.unaligned" points to the beginning of this unaligned buffer and
 * "dio_buff.aligned" points to an offset within this unaligned buffer that is also OS_PAGE_SIZE aligned.
 */
THREADGBLDEF(dio_buff,				dio_buff_t)
#ifdef GTM_TRIGGER
THREADGBLDEF(gvt_triggers_read_this_tn,		boolean_t)			/* if non-zero, indicates triggers were read for
										 * at least one gv_target in this transaction.
										 * A complete list of all gv_targets who had
										 * triggers read in this transaction can be found
										 * by going through the gvt_tp_list and checking
										 * if gvt->trig_read_tn matches local_tn. This is
										 * useful to invalidate all those triggers in case
										 * of a transaction restart or rollback.
										 */
THREADGBLDEF(op_fntext_tlevel,			uint4)				/* Non-zero implies $TEXT argument is a trigger
										 *	routine.
										 * If non-zero, this is 1 + dollar_tlevel at
										 * the time of the call to "get_src_line" in
										 * op_fntext.c. Later used by fntext_ch.c.
										 */
THREADGBLDEF(in_op_fntext,			boolean_t)			/* Denote the trigger was processed in $Text() */
THREADGBLDEF(ztrigbuff,				char *)				/* Buffer to hold $ztrigger/mupip-trigger output
										 * until TCOMMIT.
										 */
THREADGBLDEF(ztrigbuffAllocLen,			int)				/* Length of allocated ztrigbuff buffer */
THREADGBLDEF(ztrigbuffLen,			int)				/* Length of used ztrigbuff buffer */
THREADGBLDEF(ztrig_use_io_curr_device,		boolean_t)	/* Use current IO device instead of stderr/util_out_print */
#endif

THREADGBLDEF(in_ext_call,			boolean_t)	/* Indicates we are in an external call */

/* linux AIO related data */
#ifdef 	USE_LIBAIO
THREADGBLDEF(ydb_aio_nr_events,			uint4)		/* Indicates the value of the nr_events parameter suggested for
								 * use by io_setup().
								 */
#endif
THREADGBLDEF(crit_reg_count,			int4)		/* A count of the number of regions/jnlpools where this process
								 * has crit
								 */
THREADGBLDEF(ok_to_see_statsdb_regs,		boolean_t)	/* FALSE implies statsdb regions are hidden at "gd_load" time */
THREADGBLDEF(was_open_reg_seen,			boolean_t)	/* TRUE => there is at least one region with reg->was_open = TRUE */
THREADGBLDEF(nontp_jbuf_rsrv,			jbuf_rsrv_struct_t *)	/* Pointer to structure corresponding to reservations
									 * on the journal buffer for current non-TP transaction.
									 */
THREADGBLDEF(last_gvquery_key,			gv_key *)	/* Last key returned by $query(gvn). Note: Only one value
								 * maintained for both forward and reverse $query(gvn)
								 * This is the gv equivalent of last_fnquery_return_varname et al.
								 */
THREADGBLAR1DEF(ydbmsgprefixbuf,		char,	32)	/* The message prefix buffer size is chosen to allow at least
								 * 8 4-byte unicode characters or 32 ascii characters.
								 */
THREADGBLDEF(ydbmsgprefix,			mstr)		/* mstr pointing to msgprefixbuf containing the YDB prompt */
THREADGBLDEF(trig_forced_unwind,		boolean_t)	/* set/used by "gtm_trigger_fini", "op_unwind" and "unw_mv_ent" */
THREADGBLDEF(ydb_recompile_newer_src,		boolean_t)	/* set based on env var "ydb_recompile_newer_src" */
THREADGBLDEF(source_line,			int4)		/* keep track of line number in M file while compiling */
/* Debug values */
#ifdef DEBUG
THREADGBLDEF(LengthReentCnt,			boolean_t)	/* Reentrancy count for GetPieceCountFromPieceCache() used by 2
								 * argument $LENGTH() calls */
THREADGBLDEF(ZLengthReentCnt,			boolean_t)	/* Reentrancy count for ZGetPieceCountFromPieceCache() used by 2
								 * argument $ZLENGTH() calls */
THREADGBLDEF(donot_commit,			boolean_t)			/* debug-only - see gdsfhead.h for purpose */
THREADGBLDEF(continue_proc_cnt,			int)				/* Used by whitebox secshr test to count time
										 * process was continued. */
THREADGBLDEF(ydb_test_fake_enospc,		boolean_t)			/*  DEBUG-only option to enable/disable anticipatory
										 *  freeze fake ENOSPC testing
										 */
THREADGBLDEF(ydb_test_jnlpool_sync,		uint4)				/*  DEBUG-only option to force the journal pool
										 *  accounting out of sync every n transactions.
										 */
THREADGBLDEF(ydb_usesecshr,			boolean_t)			/* Bypass easy methods of dealing with IPCs, files,
										 * wakeups, etc and always use gtmsecshr (testing).
										 */
THREADGBLDEF(rts_error_unusable,		boolean_t)			/* Denotes the window in which an rts_error is
										 * unusable */
THREADGBLDEF(rts_error_unusable_seen,		boolean_t)
THREADGBLAR1DEF(trans_restart_hist_array,	trans_restart_hist_t, TRANS_RESTART_HIST_ARRAY_SZ) /* See tp.h for usage */
THREADGBLDEF(trans_restart_hist_index,		uint4)
THREADGBLDEF(skip_mv_num_approx_assert,		boolean_t)		/* TRUE if mval2subsc is invoked from op_fnview */
THREADGBLDEF(ydb_gvundef_fatal,			boolean_t)			/* core and die intended for testing */
THREADGBLDEF(ydb_lockhash_n_bits,		uint4)		/* = N => Restrict hash of lock subscripts to at most N bits */
THREADGBLDEF(ydb_dirtree_collhdr_always,	boolean_t)	/* Initialize 4-byte collation header in directory tree always.
								 * Used by tests that are sensitive to DT leaf block layout.
								 */
THREADGBLDEF(activelv_cycle,			int)			/* # of times SET_ACTIVE_LV macro has been invoked */
THREADGBLDEF(activelv_index,			int)			/* == (activelv_cycle % ACTIVELV_DBG_ARRAY_SIZE_DEF) */
THREADGBLDEF(activelv_dbg_array,		activelv_dbg_t *)	/* pointer to array holding trace details for
									 * ACTIVELV_DBG_ARRAY_SIZE_DEF most recent
									 * invocations of SET_ACTIVE_LV */
THREADGBLDEF(cli_get_str_max_len,		uint4)
# ifdef GTM_TRIGGER
THREADGBLDEF(gtmio_skip_tlevel_assert,		boolean_t)	/* Allow for "util_out_print_gtmio" calls without TP
								 * if this variable is TRUE.
								 */
THREADGBLDEF(in_trigger_upgrade,		boolean_t)	/* caller is MUPIP TRIGGER -UPGRADE */
#endif	/* #ifdef GTM_TRIGGER */
THREADGBLDEF(ydb_test_autorelink_always,	boolean_t)	/*  DEBUG-only option to enable/disable autorelink always */
THREADGBLDEF(fork_without_child_wait,		boolean_t)	/*  we did a FORK but did not wait for child to detach from
								 *  inherited shm so shm_nattch could be higher than we expect.
								 */
THREADGBLDEF(in_bm_getfree_gdsfilext,           boolean_t)	/* bm_getfree() did a preemptive crit grab before doing a
								 * file extension.
								 */
#endif	/* #ifdef DEBUG */
/* (DEBUG_ONLY relevant points reproduced from the comment at the top of this file)
 *   5. It is important for ANY DEBUG_ONLY fields to go at the VERY END. Failure to do this breaks gtmpcat.
 *   6. If a DEBUG_ONLY array is declared whose dimension is a macro, then it is necessary, for gtmpcat to work,
 *      that the macro shouldn't be defined as DEBUG_ONLY.
 */
