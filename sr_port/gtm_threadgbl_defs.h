/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
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
 *      b. Gorup related items (such as compiler, replication, etc.
 *   2. Because references will use an offset into this structure and since the "immediate offset" in
 *      compiler generated instructions is usually limited to "smaller" values like less than 16K or
 *      32K or whatever (platform dependent), items near the top of the table should be reserved used for
 *      the most commonly used items (e.g. cs_addrs, cs_data, gv_target, etc).
 *   3. Larger arrays/structures should go nearer the end.
 *   4. There are no other runtime dependencies on this order. The order of fields can be switched around
 *      any way desired with just a rebuild.
 */

/* Priority access fields - commonly used fields in performance situations */
THREADGBLDEF(grabbing_crit, 			gd_region *)			/* Region currently grabbing crit in (if any) */

/* Compiler */
THREADGBLDEF(boolchain,				triple)				/* anchor for chain used by bx_boolop  */
THREADGBLDEF(boolchain_ptr,			triple *)			/* pointer to anchor for chain used by bx_boolop  */
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
THREADGBLDEF(for_nest_level,			uint4)				/* kludge feeds extra (non-lvn) arg to FOR rt ops */
THREADGBLDEF(for_stack_ptr,			oprtype **)			/* part of FOR compilation nesting mechanism */
THREADGBLDEF(gtm_fullbool,			unsigned int)			/* controls boolean side-effect behavior defaults
										 * to 0 (GTM_BOOL) */
THREADGBLDEF(ind_result_array,			mval **)			/* base of array holding indirection returns */
THREADGBLDEF(ind_result_sp,			mval **)			/* pointer into array holding indirection returns */
THREADGBLDEF(ind_result_top,			mval **)			/* top of array holding indirection returns */
THREADGBLDEF(ind_source_array,			mval **)			/* base of array holding indirection values */
THREADGBLDEF(ind_source_sp,			mval **)			/* pointer into array holding indirection values */
THREADGBLDEF(ind_source_top,			mval **)			/* top of array holding indirection values */
THREADGBLDEF(last_source_column,		short int)			/* parser tracker */
THREADGBLDEF(pos_in_chain,			triple)				/* anchor used to restart after a parsing error */
THREADGBLDEF(s2n_intlit, 			boolean_t)			/* type info from s2n for advancewindow */
THREADGBLDEF(saw_side_effect,			boolean_t)			/* need side effect handling other than naked */
THREADGBLDEF(shift_side_effects, 		int)				/* flag shifting of side-effects ahead of boolean
										 * evalation */
THREADGBLDEF(source_error_found,		int4)				/* flag to partially defer compiler error */
THREADGBLDEF(temp_subs,				boolean_t)			/* flag temp storing of subscripts to preserve
										 * current evaluation */
THREADGBLDEF(trigger_compile,			boolean_t)			/* A trigger compilation is active */
THREADGBLDEF(window_ident,			mstr)				/* current scanner mident from advancewindow */
THREADGBLDEF(window_mval,			mval)				/* current scanner mval from advancewindow */
THREADGBLDEF(window_token,			char)				/* current scanner token from advancewindow */

/* Database */
THREADGBLDEF(dbinit_max_hrtbt_delta,		uint4)				/* max heartbeats before we bail out in db_init */
THREADGBLDEF(donot_commit,			boolean_t)			/* debug-only - see gdsfhead.h for purpose */
THREADGBLDEF(donot_write_inctn_in_wcs_recover,	boolean_t)			/* TRUE if wcs_recover should NOT write INCTN */
THREADGBLDEF(gd_targ_addr,			gd_addr *)			/* current global directory reference */
THREADGBLDEF(gtm_gvundef_fatal,			boolean_t)			/* core and die intended for testing */
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
THREADGBLDEF(gv_tporigkey_ptr,			gv_orig_key_array *)		/* op_tstart tp nesting anchor */
THREADGBLDEF(in_gvcst_redo_root_search,		boolean_t)			/* TRUE if gvcst_redo_root_search is in C-stack */
THREADGBLDEF(in_op_gvget,			boolean_t)			/* TRUE if op_gvget() is a C-stack call ancestor */
THREADGBLDEF(issue_DBROLLEDBACK_anyways,	boolean_t)			/* currently set by MUPIP LOAD */
THREADGBLDEF(last_fnquery_return_subcnt,	int)				/* count subscript in last_fnquery_return_sub */
THREADGBLDEF(last_fnquery_return_varname,	mval)				/* returned varname of last $QUERY() */
THREADGBLDEF(new_dbinit_ipc,			int4)				/* indicates whether shared memory/semaphore is
										 * created by db_init (also used by dbinit_ch) */
THREADGBLDEF(ok_to_call_wcs_recover,		boolean_t)			/* Set to TRUE before a few wcs_recover callers.
										 * Any call to wcs_recover in the final retry
										 * assert to prevent cache recovery while in a
										 * transaction and confuse things enough to cause
										 * further restarts (which is out-of-design while
										 * in the final retry). */
THREADGBLDEF(only_reset_clues_if_onln_rlbk,	boolean_t)			/* skip online rollback cleanup on a restart */
THREADGBLDEF(prev_gv_target,			gv_namehead *)			/* saves the last gv_target for debugging */
THREADGBLDEF(ready2signal_gvundef,		boolean_t)			/* TRUE if GET operation about to signal GVUNDEF */
THREADGBLDEF(semwait2long,			volatile boolean_t)		/* Waited too long for a semaphore */
THREADGBLDEF(tp_restart_count,			uint4)				/* tp_restart counter */
THREADGBLDEF(tp_restart_dont_counts,		uint4)				/* tp_restart count adjustment */
THREADGBLDEF(tp_restart_entryref,		mval)				/* tp_restart position for reporting */
THREADGBLDEF(tp_restart_failhist_indx,		int4)				/* tp_restart dbg restart history index */
THREADGBLDEF(tp_restart_needlock_cnt,		uint4)				/* tp_restart final try counter */
THREADGBLDEF(tp_restart_needlock_tn,		trans_num)			/* tp_restart final try tn */
THREADGBLDEF(tprestart_syslog_delta,		int4)				/* defines every n-th restart to be logged */
THREADGBLDEF(tprestart_syslog_limit,		int4)				/* # of TP restarts logged unconditionally */
#ifdef ENABLE_EXTENDED_RESTART_TRACE_HIST
THREADGBLDEF(trans_restart_hist_index,		uint4)
#endif
THREADGBLDEF(transform,				boolean_t)			/* flag collation transform eligible */
THREADGBLDEF(wcs_recover_done,			boolean_t)			/* TRUE if wcs_recover was ever invoked in this
										 * process. */

/* Local variables */
THREADGBLDEF(in_op_fnnext,			boolean_t)			/* set TRUE by op_fnnext; FALSE by op_fnorder */
THREADGBLDEF(local_collseq,			collseq *)			/* pointer to collation algorithm for lvns */
THREADGBLDEF(local_collseq_stdnull,		boolean_t)			/* flag temp controlling empty-string subscript
										 * handling - if true, use standard null subscript
										 * collation for local variables */
THREADGBLDEF(lv_null_subs,			int)				/* UNIX: set in gtm_env_init_sp(),
										 * VMS: set in gtm$startup() */
THREADGBLDEF(max_lcl_coll_xform_bufsiz,		int)				/* max size of local collation buffer,which extends
										 * from 32K each time the buffer overflows */

/* Replication variables */
THREADGBLDEF(replgbl,				replgbl_t)			/* set of global variables needed by the source
										 * server */
/* Miscellaneous */
THREADGBLDEF(collseq_list,			collseq *)			/* list of pointers to currently mapped collation
										 * algorithms - since this seems only used in
										 * collseq.c -seems more like a STATICDEF */
THREADGBLFPTR(create_fatal_error_zshow_dmp_fptr, void, 		(void))		/* Fptr for gtm_fatal_error* zshow dmp routine */
THREADGBLDEF(disable_sigcont,			boolean_t)			/* indicates whether the SIGCONT signal
										 * is allowed internally */
THREADGBLDEF(dollar_zcompile,			mstr)				/* compiler qualifiers */
THREADGBLDEF(dollar_zmode,			mval)				/* run mode indicator */
THREADGBLDEF(dollar_zonlnrlbk,			int)				/* ISV (incremented for every online rollback) */
THREADGBLDEF(dollar_zroutines,			mstr)				/* routine search list */
THREADGBLDEF(error_on_jnl_file_lost,		unsigned int)			/* controls error handling done by jnl_file_lost.
										 * 0 (default) : Turn off journaling and continue.
										 * 1 : Keep journaling on, throw rts_error.
										 * VMS does not supports this and requires it to
										 * be 0. */
#ifdef UNIX
THREADGBLDEF(fnzsearch_lv_vars,			lv_val *)			/* UNIX op_fnzsearch lv tree anchor */
THREADGBLDEF(fnzsearch_sub_mval,		mval)				/* UNIX op_fnzsearch subscript constuctor */
THREADGBLDEF(fnzsearch_nullsubs_sav,		int)				/* UNIX op_fnzsearch temp for null subs control */
#endif
THREADGBLDEF(gtm_env_init_done,			boolean_t)			/* gtm_env_init flag for completion */
THREADGBLFPTR(gtm_env_xlate_entry,		int,		())		/* gtm_env_xlate() function pointer */
THREADGBLDEF(gtm_environment_init,		boolean_t)			/* indicates GT.M development environment rather
										 * than a production environment */
THREADGBLFPTR(gtm_sigusr1_handler,		void, 		(void))		/* SIGUSR1 signal handler function ptr */
THREADGBLDEF(gtm_trctbl_cur,			trctbl_entry *)			/* Current gtm trace table entry */
THREADGBLDEF(gtm_trctbl_end,			trctbl_entry *)			/* End of gtm trace table (last entry + 1) */
THREADGBLDEF(gtm_trctbl_groups,			unsigned int)			/* Trace group mask (max 31 groups) */
THREADGBLDEF(gtm_trctbl_start,			trctbl_entry *)			/* Start of gtm trace table */
THREADGBLDEF(gtm_waitstuck_script,		mstr)				/* Path to the script to be executed during waits*/
THREADGBLDEF(gtmprompt,				mstr)				/* mstr pointing to prombuf containing the GTM
										 * prompt */
THREADGBLDEF(in_zwrite,				boolean_t)			/* ZWrite is active */
THREADGBLDEF(lab_proxy,				lab_tabent_proxy)		/* Placeholder storing lab_ln_ptr offset / lnr_adr
										 * pointer and has_parms value, so they are
										 * contiguous in memory */
#ifdef VMS
THREADGBLDEF(lbl_tbl_entry_index,		int)				/* Index of currently compiled label table entry */
#endif
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
#ifdef UNIX
THREADGBLDEF(open_shlib_root,			open_shlib *)			/* Anchor for open shared library list */
#endif
THREADGBLDEF(parm_pool_ptr,			parm_pool *)			/* Pointer to the parameter pool */
THREADGBLDEF(parms_cnt,                         unsigned int)                   /* Parameters count */
#ifdef UNIX
THREADGBLDEF(pipefifo_interrupt,		int)				/* count of number of times a pipe or fifo device is
										 * interrupted */
#endif
THREADGBLDEF(prof_fp,				mprof_stack_frame *)		/* Stack frame that mprof currently operates on */
THREADGBLDEF(trans_code_pop,			mval *)				/* trans_code holder for $ZTRAP popping */
THREADGBLDEF(view_ydirt_str,			char *)				/* op_view working storage for ydir* ops */
THREADGBLDEF(view_ydirt_str_len,		int4)				/* part of op_view working storage for ydir* ops */
THREADGBLDEF(zdate_form,			int4)				/* control for default $zdate() format */
THREADGBLAR1DEF(zintcmd_active,			zintcmd_active_info,	ZINTCMD_LAST)	/* interrupted timed commands */
THREADGBLDEF(zro_root,				zro_ent *)			/* Anchor for zroutines structure entry array */
#ifdef UNIX
THREADGBLDEF(zsearch_var,			lv_val *)			/* UNIX $zsearch() lookup variable */
THREADGBLDEF(zsearch_dir1,			lv_val *)			/* UNIX $zsearch() directory 1 */
THREADGBLDEF(zsearch_dir2,			lv_val *)			/* UNIX $zsearch() directory 2 */
#endif

/* Larger structures and char strings */
THREADGBLDEF(fnpca,				fnpc_area)			/* $Piece cache structure area */
THREADGBLAR1DEF(for_stack,			oprtype *,	MAX_FOR_STACK)	/* stacks FOR scope complete (compilation) addrs */
THREADGBLAR1DEF(for_temps,			boolean_t,	MAX_FOR_STACK)	/* stacked flags of FOR control value temps */
THREADGBLAR1DEF(last_fnquery_return_sub,	mval,		MAX_LVSUBSCRIPTS)/* Returned subscripts of last $QUERY() */
THREADGBLDEF(lcl_coll_xform_buff,		char *)				/* This buffer is for local collation
										 * transformations, which must not nest - i.e.
										 * a transformation routine must not call another,
										 * or itself. This kind of nesting would cause
										 * overwriting of the buffer */
#ifdef UNIX
THREADGBLAR1DEF(parm_ary,                       char *,         MAX_PARMS)      /* parameter strings buffer */
THREADGBLAR1DEF(parm_ary_len,                   int,            MAX_PARMS)      /* array element allocation length */
THREADGBLAR1DEF(parm_str_len,                   int,            MAX_PARMS)      /* parameter strings lengths */
#endif
THREADGBLAR1DEF(prombuf,			char,	(MAX_MIDENT_LEN + 1))	/* The prompt buffer size (32) would allow at
										 * least 8 Unicode characters, but since most
										 * commonly used Unicode characters only occupy up
										 * to 3 bytes, the buffer would at least
										 * accommodate 10 Unicode characters in a prompt */
THREADGBLDEF(rt_name_tbl,			hash_table_mname)		/* Routine hash table for finding $TEXT() info */
THREADGBLAR1DEF(tp_restart_failhist_arry,	char,	FAIL_HIST_ARRAY_SIZE)	/* tp_restart dbg storage of restart history */
THREADGBLAR1DEF(director_string,		char,	SIZEOF(mident_fixed))	/* buffer for director_ident */
THREADGBLAR1DEF(window_string,			char,	SIZEOF(mident_fixed))	/* buffer for window_ident */
#ifdef ENABLE_EXTENDED_RESTART_TRACE_HIST
THREADGBLAR1DEF(trans_restart_hist_array,	trans_restart_hist_t, TRANS_RESTART_HIST_ARRAY_SZ) /* See tp.h for usage */
#endif

/* GTM Call-in related globals */
#ifdef UNIX
THREADGBLDEF(callin_hashtab, 			hash_table_str *)		/* Callin hash table */
THREADGBLDEF(ci_table, 				callin_entry_list *)		/* Callin table in the form of a linked list */
#endif
THREADGBLDEF(extcall_package_root,		struct extcall_package_list *)	/* External call table package list */
#ifdef UNIX
THREADGBLDEF(gtmci_nested_level,		unsigned int)			/* Current nested depth of callin environments */
#endif

THREADGBLDEF(want_empty_gvts,			boolean_t)			/* set to TRUE by MUPIP REORG when it is selecting
										 * globals to be reorged. Need to be able to select
										 * killed globals for effective truncate. */
