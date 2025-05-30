/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_inet.h"
#include "gtm_signal.h"

#include "error.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "mv_stent.h"
#include "startup.h"
#include "cmd_qlf.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "tp.h"
#include "interlock.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "xfer_enum.h"
#include "cache.h"
#include "op.h"
#include "gt_timer.h"
#include "io.h"
#include "gtmio.h"
#include "dpgbldir_sysops.h"	/* for dpzgbini prototype */
#include "comp_esc.h"
#include "ctrlc_handler.h"
#include "get_page_size.h"
#include "sig_init.h"
#include "zcall_package.h"
#include "getzdir.h"
#include "getzmode.h"
#include "getzprocess.h"
#include "gtm_exit_handler.h"
#include "gtmmsg.h"
#include "getjobname.h"
#include "jobchild_init.h"
#include "error_trap.h"			/* for ecode_init() prototype */
#include "zyerror_init.h"
#include "trap_env_init.h"
#include "zdate_form_init.h"
#include "mstack_size_init.h"
#include "dollar_system_init.h"
#include "gtm_startup.h"
#include "svnames.h"
#include "jobinterrupt_process.h"
#include "zco_init.h"
#include "suspsigs_handler.h"
#include "ydb_logical_truth_value.h"
#include "gtm_utf8.h"
#include "gtm_icu_api.h"	/* for u_strToUpper and u_strToLower */
#include "gtm_conv.h"
#include "fix_xfer_entry.h"
#include "zwrite.h"
#include "alias.h"
#include "cenable.h"
#include "gtmimagename.h"
#include "mprof.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"
#include "jobsp.h" /* For gcall.h */
#include "gcall.h" /* For ojchildparms() */
#include "common_startup_init.h"
#include "ydb_trans_numeric.h"
#ifdef UTF8_SUPPORTED
#include "utfcgr.h"
#endif
#include "libyottadb_int.h"
#include "mdq.h"
#include "invocation_mode.h"
#include "ydb_os_signal_handler.h"
#include "deferred_events.h"
#include "get_command_line.h"

GBLDEF void			(*restart)() = &mum_tstart;
#ifdef __MVS__
/* In zOS we cann't access function address directly, So creating function pointer
 * which points to mdb_condition_handler. We can use this function pointer to get
 * the address of mdb_condition_handler in ESTABLSH assembly macro.
 */
GBLDEF ch_ret_type		(*mdb_condition_handler_ptr)(int arg) = &mdb_condition_handler;
#endif

GBLREF	rtn_tabent		*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLREF	int4			break_message_mask;
GBLREF	stack_frame 		*frame_pointer;
GBLREF	unsigned char 		*stackbase, *stacktop, *stackwarn, *msp;
GBLREF	unsigned char		*fgncal_stack;
GBLREF	mv_stent			*mv_chain;
GBLREF	xfer_entry_t		xfer_table[];
GBLREF	mval			dollar_system;
GBLREF	mval			dollar_zcmdline;
GBLREF	mval			dollar_zstatus;
GBLREF	bool			compile_time;
GBLREF	spdesc			stringpool;
GBLREF	spdesc			rts_stringpool;
GBLREF	command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF	symval			*curr_symval;
GBLREF	boolean_t		is_replicator;
GBLREF	void			(*ctrlc_handler_ptr)();
GBLREF	boolean_t		mstr_native_align;
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	casemap_t		casemaps[];
GBLREF	void             	(*cache_table_relobjs)(void);   /* Function pointer to call cache_table_rebuild() */
GBLREF	ch_ret_type		(*ht_rhash_ch)();		/* Function pointer to hashtab_rehash_ch */
GBLREF	ch_ret_type		(*jbxm_dump_ch)();		/* Function pointer to jobexam_dump_ch */
GBLREF	ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */
GBLREF	enum gtmImageTypes	image_type;
GBLREF	int			init_xfer_table(void);
GBLREF	void			(*ydb_stm_thread_exit_fnptr)(void);
GBLREF	void			(*ydb_stm_invoke_deferred_signal_handler_fnptr)(void);
GBLREF	boolean_t		(*xfer_set_handlers_fnptr)(int4, int4 param, boolean_t popped_entry);
GBLREF	pthread_mutex_t		ydb_engine_threadsafe_mutex[STMWORKQUEUEDIM];
GBLREF	pthread_t		ydb_engine_threadsafe_mutex_holder[STMWORKQUEUEDIM];
GBLREF	boolean_t		ydb_treat_sigusr2_like_sigusr1;

OS_PAGE_SIZE_DECLARE

#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256

void gtm_startup(struct startup_vector *svec)
{	/* initialize various process characteristics and states, but beware as initialization occurs in other places as well
	 * svec is really a VMS vestige as it had a file to tailor process characteristics
	 * while in UNIX, it's all done with environment variables
	 * hence, various references to data copied from *svec could profitably be referenced directly
	 */
	boolean_t		is_defined;
	char			*temp;
	int4			temp_ydb_strpllim;
	stack_frame 		*frame_pointer_lcl;
	static char 		other_mode_buf[] = "OTHER";
	int			i, status;
	boolean_t		ret;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INVALID_IMAGE != image_type);
	assert(svec->argcnt == SIZEOF(*svec));
	IA64_ONLY(init_xfer_table());
	get_page_size();
	cache_table_relobjs = &cache_table_rebuild;
	INIT_FNPTR_GLOBAL_VARIABLES;
	ht_rhash_ch = &hashtab_rehash_ch;
	jbxm_dump_ch = &jobexam_dump_ch;
	stpgc_ch = &stp_gcol_ch;
	rtn_fst_table = rtn_names = (rtn_tabent *)svec->rtn_start;
	rtn_names_end = rtn_names_top = (rtn_tabent *)svec->rtn_end;
	mstack_size_init(svec);
	/* mark the stack base so that if error occur during call-in gtm_init(), the unwind
	 * logic in gtmci_ch() will get rid of the stack completely
	 */
	fgncal_stack = stackbase;
	mv_chain = (mv_stent *)msp;
	mv_chain->mv_st_type = MVST_STORIG;	/* Initialize first (anchor) mv_stent so doesn't do anything */
	mv_chain->mv_st_next = 0;
	mv_chain->mv_st_cont.mvs_storig = 0;
	break_message_mask = svec->break_message_mask;
	if (svec->user_strpl_size < STP_INITSIZE)
		svec->user_strpl_size = STP_INITSIZE;
	else if (svec->user_strpl_size > STP_MAXINITSIZE)
		svec->user_strpl_size = STP_MAXINITSIZE;
	/* Note: It is possible for stringpool to be already allocated if caller is "ydb_init". It calls "stp_init"
	 *	First call  : "ydb_init" -> "stp_init"
	 *	Second call : "ydb_init" -> "init_gtm" -> "gtm_startup" -> "stp_init"
	 * If we are the second call, we should be able to skip the "stp_init". But one cannot be sure the two calls
	 * passed the same stringpool size. So better to free the first one and allocate the second one afresh since
	 * this is what is going to be used for the rest of the process now that "ydb_init" is done with its stringpool need.
	 */
	if (NULL != stringpool.top)
		free(stringpool.base);
	stp_init(svec->user_strpl_size);
	assertpro(stringpool.base);
	rts_stringpool = stringpool;
	(TREF(tpnotacidtime)).mvtype = MV_NM | MV_INT;	/* gtm_env_init set up a numeric value, now there's a stp: string it */
	MV_FORCE_STRD(&(TREF(tpnotacidtime)));
	assert(6 >= (TREF(tpnotacidtime)).str.len);
	temp = malloc((TREF(tpnotacidtime)).str.len);
	memcpy((void *)temp, (TREF(tpnotacidtime)).str.addr, (TREF(tpnotacidtime)).str.len);
	(TREF(tpnotacidtime)).str.addr = temp;
	TREF(compile_time) = FALSE;
	/* assert that is_replicator and run_time is properly set by gtm_imagetype_init invoked at process entry */
#	ifdef DEBUG
	switch (image_type)
	{
		case GTM_IMAGE:
			assert(is_replicator && run_time);
			break;
		case MUPIP_IMAGE:
			assert(!is_replicator && !run_time);
			break;
		default:
			assert(FALSE);
	}
#	endif
	gtm_utf8_init(); /* Initialize the runtime for UTF8 */
	/* Initialize alignment requirement for the runtime stringpool */
	/* mstr_native_align = ydb_logical_truth_value(YDBENVINDX_DISABLE_ALIGNSTR, FALSE, NULL) ? FALSE : TRUE; */
	mstr_native_align = FALSE; /* TODO: remove this line and uncomment the above line */
	/* See if $ydb_stp_gcol_nosort is set */
	ret = ydb_logical_truth_value(YDBENVINDX_STP_GCOL_NOSORT, FALSE, &is_defined);
	stringpool.stp_gcol_nosort = (is_defined ? ret : FALSE);
	/* See if $ydb_string_pool_limit is set */
	temp_ydb_strpllim = ydb_trans_numeric(YDBENVINDX_STRING_POOL_LIMIT,  &is_defined, IGNORE_ERRORS_FALSE, NULL);
	if (0 < temp_ydb_strpllim)
		stringpool.strpllim = (temp_ydb_strpllim < STP_GCOL_TRIGGER_FLOOR) ? STP_GCOL_TRIGGER_FLOOR :  temp_ydb_strpllim;
	getjobname();
	getzprocess();
	getzmode();
	zcall_init();
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	cache_init();
	/* Put a base frame on the stack. One assumes this base frame is so there is *something* on the stack in case an error
	 * or some such gets driven that looks at the stack. Needs to be at least one frame there. However, once we invoke
	 * gtm_init_env (via jobchild_init()), this frame is no longer reachable since it builds the "real" base frame.
	 * Note the last sentence does not apply to call-ins or to the simpleapi which do not call gtm_init_env().
	 */
	msp -= SIZEOF(stack_frame);
	frame_pointer_lcl = (stack_frame *)msp;
	memset(frame_pointer_lcl, 0, SIZEOF(stack_frame));
	frame_pointer_lcl->temps_ptr = (unsigned char *)frame_pointer_lcl;
	frame_pointer_lcl->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer_lcl->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer_lcl->type = SFT_COUNT;
	frame_pointer_lcl->rvector = (rhdtyp *)malloc(SIZEOF(rhdtyp));
	memset(frame_pointer_lcl->rvector, 0, SIZEOF(rhdtyp));
	frame_pointer = frame_pointer_lcl;
	symbinit();
	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	TREF(zsearch_var) = lv_getslot(curr_symval);
	LVVAL_INIT((TREF(zsearch_var)), curr_symval);
	/* Initialize global pointer to control-C handler. Also used in iott_use */
	ctrlc_handler_ptr = &ctrlc_handler;
	if (!IS_MUPIP_IMAGE)
	{
		DEFINE_EXIT_HANDLER(gtm_exit_handler, TRUE);
		if (!(MUMPS_CALLIN & invocation_mode))
			sig_init(ydb_os_signal_handler, ctrlc_handler_ptr, suspsigs_handler, continue_handler);
		else
		{	/* SimpleAPI/Call-in invocation of YDB. Ctrl-C should terminate the process.
			 * Treat it like SIGTERM by using "ydb_os_signal_handler" for SIGINT (Ctrl-C) too.
			 */
			if (USING_ALTERNATE_SIGHANDLING)
			{	/* If using alternate signal handling, ignore the env var that asked for SIGUSR2 to be
				 * treated like SIGUSR1. This is because SIGUSR2 (aka YDBSIGNOTIFY) has special meaning
				 * for alternate signal handling (see comments inside "sig_init_lang_altmain.c" for details).
				 */
				ydb_treat_sigusr2_like_sigusr1 = FALSE;
				sig_init_lang_altmain();
			} else
				sig_init(ydb_os_signal_handler, ydb_os_signal_handler, suspsigs_handler, continue_handler);
		}
	}
	io_init(IS_MUPIP_IMAGE);		/* starts with nocenable for GT.M runtime, enabled for MUPIP */
	if (!IS_MUPIP_IMAGE)
		cenable();	/* cenable unless the environment indicates otherwise - 2 steps because this can report errors */
	jobinterrupt_init();
	getzdir();
	INIT_ENV_AND_GBLDIR_XLATE;
	dpzgbini();
	zco_init();
	get_command_line(&dollar_zcmdline, TRUE);
	/* a base addr of 0 indicates a gtm_init call from an rpc server */
	if ((GTM_IMAGE == image_type) && (NULL != svec->base_addr))
		jobchild_init();
	else
	{	/* Trigger enabled utilities will enable through here */
		(TREF(dollar_zmode)).mvtype = MV_STR;
		(TREF(dollar_zmode)).str.addr = other_mode_buf;
		(TREF(dollar_zmode)).str.len = SIZEOF(other_mode_buf) -1;
	}
	svec->frm_ptr = (unsigned char *)frame_pointer;
	dollar_zstatus.mvtype = MV_STR;
	dollar_zstatus.str.len = 0;
	dollar_zstatus.str.addr = NULL;
	ecode_init();
	zyerror_init();
	if (IS_MUMPS_IMAGE)
		trap_env_init();
	zdate_form_init(svec);
	dollar_system_init(svec);
	ce_init();	/* initialize compiler escape processing */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	init_timers();		/* Init these now so we can do signal forwarding consistently (if/when #410 is un-reverted) */
	/* Initialize zwrite subsystem. Better to do it now when we have storage to allocate than
	 * if we fail and storage allocation may not be possible. To that end, pretend we have
	 * seen alias activity so those structures are initialized as well.
	 */
	assert(FALSE == curr_symval->alias_activity);
	curr_symval->alias_activity = TRUE;			/* Temporary during lvzwr_init() */
	lvzwr_init((enum zwr_init_types)0, (mval *)NULL);
	TREF(in_zwrite) = FALSE;
	curr_symval->alias_activity = FALSE;
	xfer_set_handlers_fnptr = &xfer_set_handlers;
	/* The below 2 function pointers will be set to non-NULL values in case of Simple Thread API in "ydb_stm_thread()" */
	ydb_stm_thread_exit_fnptr = NULL;
	ydb_stm_invoke_deferred_signal_handler_fnptr = NULL;
	/* Initialize pthread mutex structures in anticipation for Simple Thread API (ydb_*_st()) function calls.
	 * It is possible this process does no such calls (e.g. M or Simple API or YDBPython application).
	 * In that case, this allocation would go waste but it is a small amount of memory so it is considered okay.
	 * Note that it is not easy to move this allocation somewhere else only for the Simple Thread API case as the
	 * mutexes that the very first Simple Thread API call relies on are the ones initialized here.
	 * Note that this initialization routine does not have a return code so an error return code back to the caller
	 * is not currently possible. This could probably be addressed but the process-killing rts_error suffices for now.
	 */
	for (i = 1; i < STMWORKQUEUEDIM; i++)
	{
		status = pthread_mutex_init(&ydb_engine_threadsafe_mutex[i], NULL);
		if (status)
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("pthread_mutex_init()"), CALLFROM, status);
		}
	}
	/* Pick up the parms for this invocation */
	if ((GTM_IMAGE == image_type) && (NULL != svec->base_addr))
		/* We are in the grandchild at this point. This call is made to greet local variables sent from the midchild. There
		 * is no symbol table for locals before this point so we have to greet them here, after creating the symbol table.
		 */
		ojchildparms(NULL, NULL, NULL);
	if ((NULL != (TREF(mprof_env_gbl_name)).str.addr))
		turn_tracing_on(TADR(mprof_env_gbl_name), TRUE, (TREF(mprof_env_gbl_name)).str.len > 0);
	return;
}

void gtm_utf8_init(void)
{
	int	utfcgr_size, alloc_size, i;
#	ifdef UTF8_SUPPORTED
	utfcgr	*utfcgrp, *p;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!gtm_utf8_mode)
	{	/* UTF8 is not enabled (i.e. $ZCHSET="M"). All standard functions must be byte oriented */
		FIX_XFER_ENTRY(xf_setextract, op_setzextract);
		FIX_XFER_ENTRY(xf_fnj2, op_fnzj2);
		FIX_XFER_ENTRY(xf_setpiece, op_setzpiece);
		/* If optimization of this happens, we need to move this to
		 * 	expritem.c under 'update opcodes rather than mess with xfer table' */
		FIX_XFER_ENTRY(xf_fnpopulation, op_fnzpopulation);
		FIX_XFER_ENTRY(xf_setp1, op_setzp1);
		/* If optimization of this happens, we need to move this to
		 * 	expritem.c under 'update opcodes rather than mess with xfer table' */
		FIX_XFER_ENTRY(xf_fnreverse, op_fnzreverse);
		return;
	} else
	{	/* We are in UTF8 mode - allocate desired UTF8 parse cache and initialize it. This is effectively a 2 dimensional
		 * structure where both dimensions are variable.
		 */
#       	ifdef UTF8_SUPPORTED
		utfcgr_size = OFFSETOF(utfcgr, entry) + (SIZEOF(utfcgr_entry) * TREF(ydb_utfcgr_string_groups));
		alloc_size = utfcgr_size * TREF(ydb_utfcgr_strings);
		(TREF(utfcgra)).utfcgrs = utfcgrp = (utfcgr *)malloc(alloc_size);
		memset((char *)utfcgrp, 0, alloc_size);			/* Init to zeros */
		for (i = 0, p = utfcgrp; TREF(ydb_utfcgr_strings) > i; i++, p = (utfcgr *)((INTPTR_T)p + utfcgr_size))
			/* Initialize cache structure for UTF8 string scan lookaside cache */
			p->idx = i;					/* Initialize index value */
		(TREF(utfcgra)).utfcgrsize = utfcgr_size;
		(TREF(utfcgra)).utfcgrsteal = utfcgrp;			/* Starting place to look for cache reuse */
		/* Pointer to the last usable utfcgr struct */
		(TREF(utfcgra)).utfcgrmax = (utfcgr *)((UINTPTR_T)utfcgrp + ((TREF(ydb_utfcgr_strings) - 1) * utfcgr_size));
		/* Spins to find non-(recently)-referenced cache slot before we overwrite an entry */
		TREF(utfcgr_string_lookmax) = TREF(ydb_utfcgr_strings) / UTFCGR_MAXLOOK_DIVISOR;
#		endif /* UTF8_SUPPORTED */
	}
}
