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

#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <signal.h>

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "iosp.h"
#include "error.h"
#include "cli.h"
#include "stringpool.h"
#include "interlock.h"
#include "gtmimagename.h"
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
#include "repl_msg.h"
#include "gtmsource.h"
#include "util.h"
#include "gt_timer.h"
#include "lke.h"
#include "lke_fileio.h"
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "getjobname.h"
#include "getjobnum.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "suspsigs_handler.h"
#include "patcode.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"

#ifdef UNICODE_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF char			cli_err_str[];
GBLREF CLI_ENTRY		lke_cmd_ary[];

GBLDEF CLI_ENTRY		*cmd_ary = &lke_cmd_ary[0];	/* Define cmd_ary to be the LKE specific cmd table */

static bool lke_process(int argc);
static void display_prompt(void);

error_def(ERR_CTRLC);

int main (int argc, char *argv[])
{
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	set_blocksig();
	gtm_imagetype_init(LKE_IMAGE);
	gtm_wcswidth_fnptr = gtm_wcswidth;
	gtm_env_init();	/* read in all environment variables */
	licensed = TRUE;
	err_init(util_base_ch);
	UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	sig_init(generic_signal_handler, lke_ctrlc_handler, suspsigs_handler, continue_handler);
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
	region_init(TRUE);
	getjobnum();

	cli_lex_setup(argc, argv);
	/*      this should be after cli_lex_setup() due to S390 A/E conversion    */
	OPERATOR_LOG_MSG;
	while (1)
	{
		if (!lke_process(argc) || 2 <= argc)
			break;
	}
	lke_exit();
}

static bool lke_process(int argc)
{
	bool		flag = FALSE;
	int		res;
	static int	save_stderr = SYS_STDERR;

	ESTABLISH_RET(util_ch, TRUE);
	if (util_interrupt)
		rts_error(VARLSTCNT(1) ERR_CTRLC);
	if (SYS_STDERR != save_stderr)  /* necesary in case of rts_error */
		close_fileio(&save_stderr);
	assert(SYS_STDERR == save_stderr);

	func = 0;
	util_interrupt = 0;
 	if (argc < 2)
		display_prompt();
	if ( EOF == (res = parse_cmd()))
	{
		if (util_interrupt)
		{
			rts_error(VARLSTCNT(1) ERR_CTRLC);
			REVERT;
			return TRUE;
		}
		else
		{
			REVERT;
			return FALSE;
		}
	} else if (res)
	{
		if (1 < argc)
		{
			REVERT;
			rts_error(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		} else
			gtm_putmsg(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
	}
	if (func)
	{
		flag = open_fileio(&save_stderr); /* save_stderr = SYS_STDERR if -output option not present */
		func();
		if (flag)
			close_fileio(&save_stderr);
		assert(SYS_STDERR == save_stderr);
	}
	REVERT;
	return(1 >= argc);
}

static void display_prompt(void)
{
	PRINTF("LKE> ");
	FFLUSH(stdout);
}

