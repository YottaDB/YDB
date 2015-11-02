/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "mv_stent.h"
#include "startup.h"
#include "cmd_qlf.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
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
#include "ztrap_new_init.h"
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

#ifdef __sun
#define PACKAGE_ENV_TYPE  "GTMXC_RPC"  /* env var to use rpc instead of xcall */
#endif

#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256

static int init_xfer_table(void);

GBLDEF void		(*restart)() = &mum_tstart;

GBLREF mval 		**ind_result_array, **ind_result_sp, **ind_result_top;
GBLREF mval 		**ind_source_array, **ind_source_sp, **ind_source_top;
GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLREF int4		break_message_mask;
GBLREF stack_frame 	*frame_pointer;
GBLREF unsigned char 	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF unsigned char	*fgncal_stack;
GBLREF mv_stent		*mv_chain;
GBLREF xfer_entry_t	xfer_table[];
GBLREF mval		dollar_system;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zmode;
GBLREF bool		compile_time;
GBLREF boolean_t	run_time;
GBLREF spdesc		stringpool;
GBLREF spdesc		rts_stringpool;
GBLREF command_qualifier glb_cmd_qlf, cmd_qlf;
GBLREF bool		lv_null_subs;
GBLREF lv_val		*zsrch_var, *zsrch_dir1, *zsrch_dir2;
GBLREF symval		*curr_symval;
GBLREF collseq          *local_collseq;
GBLREF pattern          *pattern_list;
GBLREF pattern          *curr_pattern;
GBLREF pattern          mumps_pattern;
GBLREF uint4    	*pattern_typemask;
GBLREF bool		transform;
GBLREF fnpc_area	fnpca;
GBLREF int4		exi_condition;
GBLREF global_latch_t 	defer_latch;
GBLREF boolean_t	is_replicator;
GBLREF void		(*ctrlc_handler_ptr)();
GBLREF boolean_t	mstr_native_align;
GBLREF boolean_t	gtm_utf8_mode;
GBLREF boolean_t	utf8_patnumeric;
GBLREF mstr		dollar_zchset;
GBLREF mstr		dollar_zpatnumeric;
GBLREF casemap_t	casemaps[];

OS_PAGE_SIZE_DECLARE

error_def(ERR_COLLATIONUNDEF);

void gtm_startup(struct startup_vector *svec)
/* Note: various references to data copied from *svec could profitably be referenced directly */
{
	unsigned char	*mstack_ptr;
	void		gtm_ret_code();
	static readonly unsigned char init_break[1] = {'B'};
	int4		lct;
	int		i;
	static char 	other_mode_buf[] = "OTHER";
	mstr		log_name;

	assert(svec->argcnt == sizeof(*svec));
	IA64_ONLY(init_xfer_table();)
	get_page_size();
	rtn_fst_table = rtn_names = (rtn_tabent *)svec->rtn_start;
	rtn_names_end = rtn_names_top = (rtn_tabent *)svec->rtn_end;
	if (svec->user_stack_size < 4096)
		svec->user_stack_size = 4096;
	if (svec->user_stack_size > 8388608)
		svec->user_stack_size = 8388608;
	mstack_ptr = (unsigned char *)malloc(svec->user_stack_size);
        msp = stackbase = mstack_ptr + svec->user_stack_size - sizeof(char *);

	/* mark the stack base so that if error occur during call-in gtm_init(), the unwind
	   logic in gtmci_ch() will get rid of the stack completely */
	fgncal_stack = stackbase;
	mv_chain = (mv_stent *)msp;
	stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + 1024;
	break_message_mask = svec->break_message_mask;
	lv_null_subs = svec->lvnullsubs;
	if (svec->user_strpl_size < STP_INITSIZE)
		svec->user_strpl_size = STP_INITSIZE;
	else if (svec->user_strpl_size > STP_MAXINITSIZE)
		svec->user_strpl_size = STP_MAXINITSIZE;
	stp_init(svec->user_strpl_size);
	if (svec->user_indrcache_size > MAX_INDIRECTION_NESTING || svec->user_indrcache_size < MIN_INDIRECTION_NESTING)
		svec->user_indrcache_size = MIN_INDIRECTION_NESTING;
	ind_result_array = (mval **)malloc(sizeof(int4) * svec->user_indrcache_size);
	ind_source_array = (mval **)malloc(sizeof(int4) * svec->user_indrcache_size);
	ind_result_sp = ind_result_array;
	ind_result_top = ind_result_sp + svec->user_indrcache_size;
	ind_source_sp = ind_source_array;
	ind_source_top = ind_source_sp + svec->user_indrcache_size;
	rts_stringpool = stringpool;
	compile_time = FALSE;
	run_time = TRUE;

	gtm_utf8_init(); /* Initialize the runtime for Unicode */

	/* Initialize alignment requirement for the runtime stringpool */
	log_name.addr = DISABLE_ALIGN_STRINGS;
	log_name.len = STR_LIT_LEN(DISABLE_ALIGN_STRINGS);
	/* mstr_native_align = logical_truth_value(&log_name, FALSE, NULL) ? FALSE : TRUE; */
	mstr_native_align = FALSE; /* TODO: remove this line and uncomment the above line */

	is_replicator = TRUE;	/* as GT.M goes through t_end() and can write jnl records to the jnlpool for replicated db */
	getjobname();
	INVOKE_INIT_SECSHR_ADDRS;
	getzprocess();
	getzmode();
	geteditor();
	zcall_init();
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	cache_init();
#ifdef __sun
        if (NULL != GETENV(PACKAGE_ENV_TYPE))	/* chose xcall (default) or rpc zcall */
            xfer_table[xf_fnfgncal] = (xfer_entry_t)op_fnfgncal_rpc;  /* using RPC */
#endif
	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer,0, sizeof(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer;
	frame_pointer->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->type = SFT_COUNT;
	frame_pointer->rvector = (rhdtyp*)malloc(sizeof(rhdtyp));
	memset(frame_pointer->rvector,0,sizeof(rhdtyp));
	symbinit();
	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	zsrch_var = lv_getslot(curr_symval);
	zsrch_dir1 = lv_getslot(curr_symval);
	zsrch_dir2 = lv_getslot(curr_symval);
	zsrch_var->v.mvtype = zsrch_dir1->v.mvtype = zsrch_dir2->v.mvtype = 0;
	zsrch_var->tp_var = zsrch_dir1->tp_var = zsrch_dir2->tp_var = 0;
	zsrch_var->ptrs.val_ent.children = zsrch_dir1->ptrs.val_ent.children =
		zsrch_dir2->ptrs.val_ent.children = 0;
	zsrch_var->ptrs.val_ent.parent.sym = zsrch_dir1->ptrs.val_ent.parent.sym =
		zsrch_dir2->ptrs.val_ent.parent.sym = curr_symval;
	/* Initialize global pointer to control-C handler. Also used in iott_use */
	ctrlc_handler_ptr = &ctrlc_handler;
	sig_init(generic_signal_handler, ctrlc_handler_ptr, suspsigs_handler);
	atexit(gtm_exit_handler);
	io_init(TRUE);
	jobinterrupt_init();
	getzdir();
	dpzgbini();
	zco_init();
	/* a base addr of 0 indicates a gtm_init call from an rpc server */
	if (svec->base_addr)
		jobchild_init();
	else
	{
		dollar_zmode.mvtype = MV_STR;
		dollar_zmode.str.addr = &other_mode_buf[0];
		dollar_zmode.str.len = sizeof(other_mode_buf) -1;
	}
	svec->frm_ptr = (unsigned char *)frame_pointer;
	dollar_ztrap.mvtype = MV_STR;
	dollar_ztrap.str.len = sizeof(init_break);
	dollar_ztrap.str.addr = (char *)init_break;
	dollar_zstatus.mvtype = MV_STR;
	dollar_zstatus.str.len = 0;
	dollar_zstatus.str.addr = (char *)0;
	ecode_init();
	zyerror_init();
	ztrap_form_init();
	ztrap_new_init();
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
	transform = TRUE;
	lct = find_local_colltype();
	if (lct != 0)
	{
		local_collseq = ready_collseq(lct);
		if (!local_collseq)
		{
			exi_condition = -ERR_COLLATIONUNDEF;
			gtm_putmsg(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
			exit(exi_condition);
		}
	} else
		local_collseq = 0;
	prealloc_gt_timers(); /* Preallocate some timer blocks. */
	for (i = 0; FNPC_MAX > i; i++)
	{ /* Initialize cache structure for $Piece function */
		fnpca.fnpcs[i].pcoffmax = &fnpca.fnpcs[i].pstart[FNPC_ELEM_MAX];
		fnpca.fnpcs[i].indx = i;
	}
	fnpca.fnpcsteal = &fnpca.fnpcs[0];		/* Starting place to look for cache reuse */
	fnpca.fnpcmax = &fnpca.fnpcs[FNPC_MAX - 1];	/* The last element */
	return;
}

#if defined(__ia64)

#ifdef XFER
#	undef XFER
#endif /* XFER */

#define XFER(a,b) #b

GBLDEF char *xfer_text[] = {
#include "xfer.h"
};
#include "xfer_desc.i"

/* On IA64, we want to use CODE_ADDRESS() macro, to dereference all the function pointers, before storing them in
   global array. Now doing a dereference operation, as part of initialization, is not allowed by linux/gcc (HP'a aCC
   was more tolerant towards this). So to make sure that the xfer_table is initialized correctly, before anyone
   uses it, this function is called right at the beginning of gtm_startup
*/

static int init_xfer_table()
{
	int i;

	for (i = 0; i < (sizeof(xfer_text) / sizeof(char *)); i++)
	{
		if (ASM == function_type(xfer_text[i]))
			xfer_table[i] = (int (*)())CODE_ADDRESS_ASM(xfer_table[i]);
		else
			xfer_table[i] = (int (*)())CODE_ADDRESS_C(xfer_table[i]);
	}

	return 0;
}

#endif /* __ia64 */

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

