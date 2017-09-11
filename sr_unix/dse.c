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
#include "main_pragma.h"

#include <stdarg.h>

#include "gtm_inet.h"
#include "gtm_signal.h"

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "error.h"
#include "interlock.h"
#include "gtmimagename.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "util.h"
#include "cli.h"
#include "op.h"
#include "gt_timer.h"
#include "io.h"
#include "dse.h"
#include "compiler.h"
#include "patcode.h"
#include "lke.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "dse_exit.h"
#include "getjobname.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "suspsigs_handler.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "wbox_test_init.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"
#include "restrict.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF gd_region		*gv_cur_region;
GBLREF gd_addr			*gd_header;
GBLREF gd_addr			*original_header;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF gv_namehead		*gv_target;
GBLREF int			(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF boolean_t		dse_running;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF char			cli_err_str[];
GBLREF boolean_t		write_after_image;
GBLREF CLI_ENTRY		dse_cmd_ary[];
GBLREF ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */

GBLDEF block_id			patch_curr_blk;
GBLDEF CLI_ENTRY		*cmd_ary = &dse_cmd_ary[0];	/* Define cmd_ary to be the DSE specific cmd table */

static bool		dse_process(int argc);
static void 		display_prompt(void);
static readonly char	prompt[]="DSE> ";

error_def(ERR_CTRLC);
error_def(ERR_RESTRICTEDOP);

int main(int argc, char *argv[])
{
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(DSE_IMAGE);
	licensed = TRUE;
	TREF(transform) = TRUE;
	TREF(no_spangbls) = TRUE;	/* dse operates on a per-region basis irrespective of global mapping in gld */
	TREF(skip_file_corrupt_check) = TRUE;	/* do not let csd->file_corrupt flag cause errors in dse */
	op_open_ptr = op_open;
	INIT_FNPTR_GLOBAL_VARIABLES;
	patch_curr_blk = get_dir_root();
	err_init(util_base_ch);
	UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	sig_init(generic_signal_handler, dse_ctrlc_handler, suspsigs_handler, continue_handler);
	atexit(util_exit_handler);
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	stp_init(STP_INITSIZE);
	stpgc_ch = &stp_gcol_ch;
	rts_stringpool = stringpool;
	getjobname();
	io_init(TRUE);
	getzdir();
	gtm_chk_dist(argv[0]);
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	initialize_pattern_table();
	if (RESTRICTED(dse))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "DSE");
	gvinit();
	region_init(FALSE);
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	cli_lex_setup(argc, argv);
	/* Since DSE operates on a region-by-region basis (for the most part), do not use a global directory at all from now on */
	original_header = gd_header;
	gd_header = NULL;
	OPERATOR_LOG_MSG;
#	ifdef DEBUG
	if ((gtm_white_box_test_case_enabled && (WBTEST_SEMTOOLONG_STACK_TRACE == gtm_white_box_test_case_number) ))
	{
		sgmnt_addrs     * csa;
		node_local_ptr_t cnl;
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		cnl = csa->nl;
		cnl->wbox_test_seq_num  = 1; /*Signal the first step and wait here*/
		/* The signal to the shell. MUPIP must not start BEFORE DSE */
		util_out_print("DSE is ready. MUPIP can start. Note: This message is a part of WBTEST_SEMTOOLONG_STACK_TRACE test. "
			       "It will not appear in PRO version.", TRUE);
		while (2 != cnl->wbox_test_seq_num) /*Wait for another process to get hold of the semaphore and signal next step*/
			LONG_SLEEP(1);
	}
#	endif
	if (argc < 2)
                display_prompt();
	while (1)
	{
		if (!dse_process(argc))
			break;
		display_prompt();
	}
	dse_exit();
	REVERT;
	return 0;
}

static void display_prompt(void)
{
	PRINTF("DSE> ");
	FFLUSH(stdout);
}

static bool	dse_process(int argc)
{
	int	res;

	ESTABLISH_RET(util_ch, TRUE);
	func = 0;
	util_interrupt = 0;
	if (EOF == (res = parse_cmd()))
	{
		if (util_interrupt)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
			REVERT;
			return TRUE;
		} else
		{
			REVERT;
			return FALSE;
		}
	} else if (res)
	{
		if (1 < argc)
		{
			/* Here we need to REVERT since otherwise we stay in dse in a loop
			 * The design of dse needs to be changed to act like VMS (which is:
			 * if there is an error in the dse command (dse dumpoa), go to command
			 * prompt, but UNIX exits
			 */
			REVERT;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		} else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
	}
	if (func)
		func();
	REVERT;
	return(1 >= argc);
}
