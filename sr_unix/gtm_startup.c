/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#include <signal.h>

#include "error.h"
#include "fnpc.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "mv_stent.h"
#include "startup.h"
#include "cmd_qlf.h"
#include "lv_val.h"
#include "collseq.h"
#include "patcode.h"
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
#include "init_secshr_addrs.h"
#include "zcall_package.h"
#include "geteditor.h"
#include "getzdir.h"
#include "getzmode.h"
#include "getzprocess.h"
#include "gtm_exit_handler.h"
#include "gtmmsg.h"
#include "getjobname.h"
#include "jobchild_init.h"
#include "error_trap.h"			/* for ecode_init() prototype */
#include "zyerror_init.h"
#include "ztrap_form_init.h"
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
#include "heartbeat_timer.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"

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
GBLREF pattern         		*pattern_list;
GBLREF pattern        		*curr_pattern;
GBLREF pattern        		mumps_pattern;
GBLREF uint4    		*pattern_typemask;
GBLREF int4			exi_condition;
GBLREF global_latch_t 		defer_latch;
GBLREF boolean_t		is_replicator;
GBLREF void			(*ctrlc_handler_ptr)();
GBLREF boolean_t		mstr_native_align;
GBLREF boolean_t		gtm_utf8_mode;
GBLREF boolean_t		utf8_patnumeric;
GBLREF mstr			dollar_zchset;
GBLREF mstr			dollar_zpatnumeric;
GBLREF casemap_t		casemaps[];
GBLREF void             	(*cache_table_relobjs)(void);   /* Function pointer to call cache_table_rebuild() */
GBLREF ch_ret_type		(*ht_rhash_ch)();		/* Function pointer to hashtab_rehash_ch */
GBLREF ch_ret_type		(*jbxm_dump_ch)();		/* Function pointer to jobexam_dump_ch */
GBLREF ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */
GBLREF enum gtmImageTypes	image_type;
GBLREF int			init_xfer_table(void);
GBLREF void			(*heartbeat_timer_ptr)(void);

OS_PAGE_SIZE_DECLARE

error_def(ERR_COLLATIONUNDEF);

#ifdef __sun
#define PACKAGE_ENV_TYPE  "GTMXC_RPC"  /* env var to use rpc instead of xcall */
#endif

#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256

void gtm_startup(struct startup_vector *svec)
/* Note: various references to data copied from *svec could profitably be referenced directly */
{
	unsigned char	*mstack_ptr;
	void		gtm_ret_code();
	int4		lct;
	int		i;
	static char 	other_mode_buf[] = "OTHER";
	mstr		log_name;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INVALID_IMAGE != image_type);
	assert(svec->argcnt == SIZEOF(*svec));
	IA64_ONLY(init_xfer_table());
	get_page_size();
	cache_table_relobjs = &cache_table_rebuild;
	ht_rhash_ch = &hashtab_rehash_ch;
	jbxm_dump_ch = &jobexam_dump_ch;
	heartbeat_timer_ptr = &heartbeat_timer;
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
	TREF(compile_time) = FALSE;
	/* assert that is_replicator and run_time is properly set by gtm_imagetype_init invoked at process entry */
#	ifdef DEBUG
	switch (image_type)
	{
		case GTM_IMAGE:
		case GTM_SVC_DAL_IMAGE:
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
	INVOKE_INIT_SECSHR_ADDRS;
	getzprocess();
	getzmode();
	geteditor();
	zcall_init();
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	cache_init();
#	ifdef __sun
	if (NULL != GETENV(PACKAGE_ENV_TYPE))	/* chose xcall (default) or rpc zcall */
		xfer_table[xf_fnfgncal] = (xfer_entry_t)op_fnfgncal_rpc;  /* using RPC */
#	endif
	msp -= SIZEOF(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer,0, SIZEOF(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer;
	frame_pointer->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->type = SFT_COUNT;
	frame_pointer->rvector = (rhdtyp*)malloc(SIZEOF(rhdtyp));
	memset(frame_pointer->rvector, 0, SIZEOF(rhdtyp));
	symbinit();
	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	TREF(zsearch_var) = lv_getslot(curr_symval);
	TREF(zsearch_dir1) = lv_getslot(curr_symval);
	TREF(zsearch_dir2) = lv_getslot(curr_symval);
	LVVAL_INIT((TREF(zsearch_var)), curr_symval);
	LVVAL_INIT((TREF(zsearch_dir1)), curr_symval);
	LVVAL_INIT((TREF(zsearch_dir2)), curr_symval);
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
	ztrap_form_init();
	zdate_form_init(svec);
	dollar_system_init(svec);
	init_callin_functable();
	gtm_env_xlate_init();
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask;
	initialize_pattern_table();
	ce_init();	/* initialize compiler escape processing */
	/* Initialize local collating sequence */
	TREF(transform) = TRUE;
	lct = find_local_colltype();
	if (lct != 0)
	{
		TREF(local_collseq) = ready_collseq(lct);
		if (!TREF(local_collseq))
		{
			exi_condition = -ERR_COLLATIONUNDEF;
			gtm_putmsg(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
			exit(exi_condition);
		}
	} else
		TREF(local_collseq) = 0;
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	for (i = 0; FNPC_MAX > i; i++)
	{	/* Initialize cache structure for $Piece function */
		(TREF(fnpca)).fnpcs[i].pcoffmax = &(TREF(fnpca)).fnpcs[i].pstart[FNPC_ELEM_MAX];
		(TREF(fnpca)).fnpcs[i].indx = i;
	}
	(TREF(fnpca)).fnpcsteal = (TREF(fnpca)).fnpcs;		/* Starting place to look for cache reuse */
	(TREF(fnpca)).fnpcmax = &(TREF(fnpca)).fnpcs[FNPC_MAX - 1];	/* The last element */
	/* Initialize zwrite subsystem. Better to do it now when we have storage to allocate than
	 * if we fail and storage allocation may not be possible. To that end, pretend we have
	 * seen alias acitivity so those structures are initialized as well.
	 */
	assert(FALSE == curr_symval->alias_activity);
	curr_symval->alias_activity = TRUE;
	lvzwr_init((enum zwr_init_types)0, (mval *)NULL);
	curr_symval->alias_activity = FALSE;
	if ((NULL != (TREF(mprof_env_gbl_name)).str.addr))
		turn_tracing_on(TADR(mprof_env_gbl_name), TRUE, (TREF(mprof_env_gbl_name)).str.len > 0);
	return;
}

void gtm_utf8_init(void)
{
	if (!gtm_utf8_mode)
	{	/* Unicode is not enabled (i.e. $ZCHSET="M"). All standard functions must be byte oriented */
	  	FIX_XFER_ENTRY(xf_fnascii, op_fnzascii)
		FIX_XFER_ENTRY(xf_fnchar, op_fnzchar)
		FIX_XFER_ENTRY(xf_fnextract, op_fnzextract)
		FIX_XFER_ENTRY(xf_setextract, op_setzextract)
		FIX_XFER_ENTRY(xf_fnfind, op_fnzfind)
		FIX_XFER_ENTRY(xf_fnj2, op_fnzj2)
		FIX_XFER_ENTRY(xf_fnlength, op_fnzlength)
		FIX_XFER_ENTRY(xf_fnpopulation, op_fnzpopulation)
		FIX_XFER_ENTRY(xf_fnpiece, op_fnzpiece)
		FIX_XFER_ENTRY(xf_fnp1, op_fnzp1)
		FIX_XFER_ENTRY(xf_setpiece, op_setzpiece)
		FIX_XFER_ENTRY(xf_setp1, op_setzp1)
		FIX_XFER_ENTRY(xf_fntranslate, op_fnztranslate)
		FIX_XFER_ENTRY(xf_fnreverse, op_fnzreverse)
		return;
	}
	dollar_zchset.len = STR_LIT_LEN(UTF8_NAME);
	dollar_zchset.addr = UTF8_NAME;
	if (utf8_patnumeric)
	{
		dollar_zpatnumeric.len = STR_LIT_LEN(UTF8_NAME);
		dollar_zpatnumeric.addr = UTF8_NAME;
	}
}

