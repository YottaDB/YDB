/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#include <netinet/in.h>
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
#include "hashdef.h"
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
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "interlock.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_env_xlate_init.h"
#include "callintogtmxfer.h"
#ifdef __sun
#include "xfer_enum.h"
#endif
#include "cache.h"
#include "op.h"
#include "gt_timer.h"
#include "io.h"
#include "dpgbldir.h"
#include "gtmio.h"
#include "dpgbldir_sysops.h"
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
#include "zyerror_init.h"
#include "ztrap_form_init.h"
#include "ztrap_new_init.h"
#include "zdate_form_init.h"
#include "dollar_system_init.h"
#include "sig_init.h"
#include "gtm_startup.h"
#include "svnames.h"
#include "gtmci_signals.h"
#include "jobinterrupt_init.h"

#ifdef __sun
#define PACKAGE_ENV_TYPE  "GTMXC_RPC"  /* env var to use rpc instead of xcall */
#endif

#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256

GBLREF mval 		**ind_result_array, **ind_result_sp, **ind_result_top;
GBLREF mval 		**ind_source_array, **ind_source_sp, **ind_source_top;
GBLREF rtn_tables 	*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLREF int4		break_message_mask;
GBLREF stack_frame 	*frame_pointer;
GBLREF unsigned char 	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF mv_stent		*mv_chain;
GBLREF int		(* volatile xfer_table[])();
GBLREF mval		dollar_system;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zstatus;
GBLREF bool		compile_time, run_time;
GBLREF spdesc		stringpool;
GBLREF spdesc		rts_stringpool;
GBLREF command_qualifier glb_cmd_qlf, cmd_qlf;
GBLREF bool		lv_null_subs;
GBLREF lv_val		*zsrch_var, *zsrch_dir1, *zsrch_dir2;
GBLREF symval		*curr_symval;
GBLREF collseq          *local_collseq;
GBLREF char		*lcl_coll_xform_buff;
GBLREF pattern          *pattern_list;
GBLREF pattern          *curr_pattern;
GBLREF pattern          mumps_pattern;
GBLREF uint4    	*pattern_typemask;
GBLREF bool		transform;
GBLREF fnpc_area	fnpca;
GBLREF sgm_info         *first_sgm_info;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char    cw_set_depth;
GBLREF uint4		process_id;
GBLREF int4		exi_condition;
GBLREF global_latch_t 	defer_latch;
GBLREF jnlpool_addrs	jnlpool;
GBLREF void		(*ctrlc_handler_ptr)();
OS_PAGE_SIZE_DECLARE

void gtm_startup(struct startup_vector *svec)
/* Note: various references to data copied from *svec could profitably be referenced directly */
{
        error_def (ERR_COLLATIONUNDEF);
	unsigned char	*mstack_ptr;
	void		gtm_ret_code();
	static readonly unsigned char init_break[1] = {'B'};
	int4		lct;
	int		i;

	assert(svec->argcnt == sizeof(*svec));
	get_page_size();
	rtn_fst_table = rtn_names = (rtn_tables *) svec->rtn_start;
	rtn_names_end = rtn_names_top = (rtn_tables *) svec->rtn_end;
	if (svec->user_stack_size < 4096)
		svec->user_stack_size = 4096;
	if (svec->user_stack_size > 8388608)
		svec->user_stack_size = 8388608;
	mstack_ptr = (unsigned char *)malloc(svec->user_stack_size);
	msp = stackbase = mstack_ptr + svec->user_stack_size - 4;
	mv_chain = (mv_stent *) msp;
	stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + 1024;
	break_message_mask = svec->break_message_mask;
	lv_null_subs = svec->lvnullsubs;
	if (svec->user_strpl_size < STP_INCREMENT)
		svec->user_strpl_size = STP_INCREMENT;
	else if (svec->user_strpl_size > STP_MAXSIZE)
		svec->user_strpl_size = STP_MAXSIZE;
	stp_init(svec->user_strpl_size);
	if (svec->user_indrcache_size > MAX_INDIRECTION_NESTING || svec->user_indrcache_size < MIN_INDIRECTION_NESTING)
		svec->user_indrcache_size = MIN_INDIRECTION_NESTING;
	ind_result_array = (mval **) malloc(sizeof(int4) * svec->user_indrcache_size);
	ind_source_array = (mval **) malloc(sizeof(int4) * svec->user_indrcache_size);
	ind_result_sp = ind_result_array;
	ind_result_top = ind_result_sp + svec->user_indrcache_size;
	ind_source_sp = ind_source_array;
	ind_source_top = ind_source_sp + svec->user_indrcache_size;
	rts_stringpool = stringpool;
	compile_time = FALSE;
	run_time = TRUE;
	getjobname();
	init_secshr_addrs(get_next_gdr, cw_set, &first_sgm_info, &cw_set_depth, process_id, OS_PAGE_SIZE,
			  &jnlpool.jnlpool_dummy_reg);
	getzprocess();
	getzmode();
	geteditor();
	zcall_init();
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	cache_init();
#ifdef __sun
        if (NULL != GETENV(PACKAGE_ENV_TYPE))	/* chose xcall (default) or rpc zcall */
            xfer_table[xf_fnfgncal] = op_fnfgncal_rpc;  /* using RPC */
#endif
	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *) msp;
	memset(frame_pointer,0, sizeof(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *) frame_pointer;
	frame_pointer->ctxt = CONTEXT(gtm_ret_code);
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
	sig_init(generic_signal_handler, ctrlc_handler_ptr);
	atexit(gtmci_exit_handler);
	atexit(gtm_exit_handler);
	io_init(TRUE);
	jobinterrupt_init();
	getzdir();
	dpzgbini();
	/* a base addr of 0 indicates a gtm_init call from an rpc server */
	if (svec->base_addr)
		jobchild_init();
	svec->frm_ptr = (unsigned char *) frame_pointer;
	dollar_ztrap.mvtype = MV_STR;
	dollar_ztrap.str.len = sizeof(init_break);
	dollar_ztrap.str.addr = (char *) init_break;
	dollar_zstatus.mvtype = MV_STR;
	dollar_zstatus.str.len = 0;
	dollar_zstatus.str.addr = (char *)0;
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
		lcl_coll_xform_buff = malloc(MAX_LCL_COLL_XFORM_BUFSIZ);
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
