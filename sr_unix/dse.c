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
#include "main_pragma.h"

#include <signal.h>
#include <stdarg.h>
#include "gtm_inet.h"

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "error.h"
#include "min_max.h"
#include "init_root_gv.h"
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
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "dse_exit.h"
#include "getjobname.h"
#include "getjobnum.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "suspsigs_handler.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"
#include "wbox_test_init.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF gd_region		*gv_cur_region;
GBLREF gd_binding		*gd_map;
GBLREF gd_binding		*gd_map_top;
GBLREF gd_addr			*gd_header;
GBLREF gd_addr			*original_header;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF gv_namehead		*gv_target;
GBLREF mval			curr_gbl_root;
GBLREF int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF boolean_t		dse_running;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF char			cli_err_str[];
GBLREF boolean_t		write_after_image;
GBLREF CLI_ENTRY		dse_cmd_ary[];

GBLDEF block_id			patch_curr_blk;
GBLDEF CLI_ENTRY		*cmd_ary = &dse_cmd_ary[0];	/* Define cmd_ary to be the DSE specific cmd table */

static bool		dse_process(int argc);
static void 		display_prompt(void);
static readonly char	prompt[]="DSE> ";

error_def(ERR_CTRLC);
int main(int argc, char *argv[])
{
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	set_blocksig();
	gtm_imagetype_init(DSE_IMAGE);
	gtm_wcswidth_fnptr = gtm_wcswidth;
	gtm_env_init();	/* read in all environment variables */
	licensed = TRUE;
	TREF(transform) = TRUE;
	op_open_ptr = op_open;
	patch_curr_blk = get_dir_root();
	err_init(util_base_ch);
	UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	sig_init(generic_signal_handler, dse_ctrlc_handler, suspsigs_handler, continue_handler);
	atexit(util_exit_handler);
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	getjobname();
	INVOKE_INIT_SECSHR_ADDRS;
	getzdir();
	gtm_chk_dist(argv[0]);
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	initialize_pattern_table();
	gvinit();
	region_init(FALSE);
	INIT_GBL_ROOT(); /* Needed for GVT initialization */
	getjobnum();
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	cli_lex_setup(argc, argv);
	CREATE_DUMMY_GBLDIR(gd_header, original_header, gv_cur_region, gd_map, gd_map_top);
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
			LONG_SLEEP(10);
	}
#	endif
	if (argc < 2)
                display_prompt();
	io_init(TRUE);
	while (1)
	{
		if (!dse_process(argc))
			break;
		display_prompt();
	}
	dse_exit();
	REVERT;
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
			rts_error(VARLSTCNT(1) ERR_CTRLC);
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
			rts_error(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		} else
			gtm_putmsg(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
	}
	if (func)
		func();
	REVERT;
	return(1 >= argc);
}
