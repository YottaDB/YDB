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

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_inet.h"
#include "gtm_signal.h"

#include "error.h"
#include <rtnhdr.h>
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_env_xlate_init.h"
#include "callintogtmxfer.h"
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
#include "generic_signal_handler.h"
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
#include "dollar_system_init.h"
#include "sig_init.h"
#include "gtm_startup.h"
#include "svnames.h"
#include "jobinterrupt_init.h"
#include "zco_init.h"
#include "gtm_logicals.h"	/* for DISABLE_ALIGN_STRINGS */
#include "suspsigs_handler.h"
#include "logical_truth_value.h"
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
#ifdef UNICODE_SUPPORTED
#include "utfcgr.h"
#endif

GBLDEF void			(*restart)() = &mum_tstart;
#ifdef __MVS__
/* In zOS we cann't access function address directly, So creating function pointer
 * which points to mdb_condition_handler. We can use this function pointer to get
 * the address of mdb_condition_handler in ESTABLSH assembly macro.
 */
GBLDEF ch_ret_type		(*mdb_condition_handler_ptr)(int arg) = &mdb_condition_handler;
#endif

GBLREF rtn_tabent		*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLREF int4			break_message_mask;
GBLREF stack_frame 		*frame_pointer;
GBLREF unsigned char 		*stackbase, *stacktop, *stackwarn, *msp;
GBLREF unsigned char		*fgncal_stack;
GBLREF mv_stent			*mv_chain;
GBLREF xfer_entry_t		xfer_table[];
GBLREF mval			dollar_system;
GBLREF mval			dollar_zstatus;
GBLREF bool			compile_time;
GBLREF spdesc			stringpool;
GBLREF spdesc			rts_stringpool;
GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF lv_val			*zsrch_var, *zsrch_dir1, *zsrch_dir2;
GBLREF symval			*curr_symval;
GBLREF global_latch_t 		defer_latch;
GBLREF boolean_t		is_replicator;
GBLREF void			(*ctrlc_handler_ptr)();
GBLREF boolean_t		mstr_native_align;
GBLREF boolean_t		gtm_utf8_mode;
GBLREF casemap_t		casemaps[];
GBLREF void             	(*cache_table_relobjs)(void);   /* Function pointer to call cache_table_rebuild() */
GBLREF ch_ret_type		(*ht_rhash_ch)();		/* Function pointer to hashtab_rehash_ch */
GBLREF ch_ret_type		(*jbxm_dump_ch)();		/* Function pointer to jobexam_dump_ch */
GBLREF ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */
GBLREF enum gtmImageTypes	image_type;
GBLREF int			init_xfer_table(void);

OS_PAGE_SIZE_DECLARE

#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256

void gtm_startup(struct startup_vector *svec)
{	/* initialize various process characteristics and states, but beware as initialization occurs in other places as well
	 * svec is really a VMS vestige as it had a file to tailor process characteristics
	 * while in UNIX, it's all done with environment variables
	 * hence, various references to data copied from *svec could profitably be referenced directly
	 */
	char		*temp;
	mstr		log_name;
	stack_frame 	*frame_pointer_lcl;
	static char 	other_mode_buf[] = "OTHER";
	unsigned char	*mstack_ptr;
	void		gtm_ret_code();
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
	if (svec->user_stack_size < 4096)
		svec->user_stack_size = 4096;
	if (svec->user_stack_size > 8388608)
		svec->user_stack_size = 8388608;
	mstack_ptr = (unsigned char *)malloc(svec->user_stack_size);
	msp = stackbase = mstack_ptr + svec->user_stack_size - mvs_size[MVST_STORIG];
	/* mark the stack base so that if error occur during call-in gtm_init(), the unwind
	 * logic in gtmci_ch() will get rid of the stack completely
	 */
	fgncal_stack = stackbase;
	mv_chain = (mv_stent *)msp;
	mv_chain->mv_st_type = MVST_STORIG;	/* Initialize first (anchor) mv_stent so doesn't do anything */
	mv_chain->mv_st_next = 0;
	mv_chain->mv_st_cont.mvs_storig = 0;
	stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + (16 * 1024);
	break_message_mask = svec->break_message_mask;
	if (svec->user_strpl_size < STP_INITSIZE)
		svec->user_strpl_size = STP_INITSIZE;
	else if (svec->user_strpl_size > STP_MAXINITSIZE)
		svec->user_strpl_size = STP_MAXINITSIZE;
	stp_init(svec->user_strpl_size);
	rts_stringpool = stringpool;
	(TREF(tpnotacidtime)).mvtype = MV_NM | MV_INT;	/* gtm_env_init set up a numeric value, now there's a stp: string it */
	MV_FORCE_STRD(&(TREF(tpnotacidtime)));
	assert(6 >= (TREF(tpnotacidtime)).str.len);
	temp = malloc((TREF(tpnotacidtime)).str.len);
	memcpy(temp, (TREF(tpnotacidtime)).str.addr, (TREF(tpnotacidtime)).str.len);
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
	gtm_utf8_init(); /* Initialize the runtime for Unicode */
	/* Initialize alignment requirement for the runtime stringpool */
	log_name.addr = DISABLE_ALIGN_STRINGS;
	log_name.len = STR_LIT_LEN(DISABLE_ALIGN_STRINGS);
	/* mstr_native_align = logical_truth_value(&log_name, FALSE, NULL) ? FALSE : TRUE; */
	mstr_native_align = FALSE; /* TODO: remove this line and uncomment the above line */
	getjobname();
	getzprocess();
	getzmode();
	zcall_init();
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	cache_init();
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
		sig_init(generic_signal_handler, ctrlc_handler_ptr, suspsigs_handler, continue_handler);
		atexit(gtm_exit_handler);
	}
	io_init(IS_MUPIP_IMAGE);		/* starts with nocenable for GT.M runtime, enabled for MUPIP */
	if (!IS_MUPIP_IMAGE)
	{
		cenable();	/* cenable unless the environment indicates otherwise - 2 steps because this can report errors */
	}
	jobinterrupt_init();
	getzdir();
	dpzgbini();
	zco_init();
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
	init_callin_functable();
	gtm_env_xlate_init();
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	ce_init();	/* initialize compiler escape processing */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	/* Initialize zwrite subsystem. Better to do it now when we have storage to allocate than
	 * if we fail and storage allocation may not be possible. To that end, pretend we have
	 * seen alias acitivity so those structures are initialized as well.
	 */
	assert(FALSE == curr_symval->alias_activity);
	curr_symval->alias_activity = TRUE;
	lvzwr_init((enum zwr_init_types)0, (mval *)NULL);
	TREF(in_zwrite) = FALSE;
	curr_symval->alias_activity = FALSE;
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
#	ifdef UNICODE_SUPPORTED
	utfcgr	*utfcgrp, *p;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!gtm_utf8_mode)
	{	/* Unicode is not enabled (i.e. $ZCHSET="M"). All standard functions must be byte oriented */
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
#       	ifdef UNICODE_SUPPORTED
		utfcgr_size = OFFSETOF(utfcgr, entry) + (SIZEOF(utfcgr_entry) * TREF(gtm_utfcgr_string_groups));
		alloc_size = utfcgr_size * TREF(gtm_utfcgr_strings);
		(TREF(utfcgra)).utfcgrs = utfcgrp = (utfcgr *)malloc(alloc_size);
		memset((char *)utfcgrp, 0, alloc_size);			/* Init to zeros */
		for (i = 0, p = utfcgrp; TREF(gtm_utfcgr_strings) > i; i++, p = (utfcgr *)((INTPTR_T)p + utfcgr_size))
			/* Initialize cache structure for UTF8 string scan lookaside cache */
			p->idx = i;					/* Initialize index value */
		(TREF(utfcgra)).utfcgrsize = utfcgr_size;
		(TREF(utfcgra)).utfcgrsteal = utfcgrp;			/* Starting place to look for cache reuse */
		/* Pointer to the last usable utfcgr struct */
		(TREF(utfcgra)).utfcgrmax = (utfcgr *)((UINTPTR_T)utfcgrp + ((TREF(gtm_utfcgr_strings) - 1) * utfcgr_size));
		/* Spins to find non-(recently)-referenced cache slot before we overwrite an entry */
		TREF(utfcgr_string_lookmax) = TREF(gtm_utfcgr_strings) / UTFCGR_MAXLOOK_DIVISOR;
#		endif /* UNICODE_SUPPORTED */
	}
}
