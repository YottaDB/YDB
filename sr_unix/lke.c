/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2001-2019 Fidelity National Information	*
=======
 * Copyright (c) 2001-2022 Fidelity National Information	*
>>>>>>> 35326517 (GT.M V7.0-003)
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"

<<<<<<< HEAD
int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "lke_main");
=======
#ifdef UTF8_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF char			cli_err_str[];
GBLREF CLI_ENTRY		lke_cmd_ary[];
GBLREF ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */
GBLREF void 			(*primary_exit_handler)(void);

GBLDEF CLI_ENTRY		*cmd_ary = &lke_cmd_ary[0];	/* Define cmd_ary to be the LKE specific cmd table */

static bool lke_process(int argc);
static void display_prompt(void);

error_def(ERR_CTRLC);

int main (int argc, char *argv[])
{
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(LKE_IMAGE);
	licensed = TRUE;
	err_init(util_base_ch);
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	sig_init(generic_signal_handler, lke_ctrlc_handler, suspsigs_handler, continue_handler);
	atexit(util_exit_handler);
	primary_exit_handler = util_exit_handler;
	stp_init(STP_INITSIZE);
	stpgc_ch = &stp_gcol_ch;
	rts_stringpool = stringpool;
	getjobname();
	getzdir();
	gtm_chk_dist(argv[0]);
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	initialize_pattern_table();
	gvinit();
	region_init(FALSE);	/* Was TRUE, but that doesn't actually work if there are GTCM regions in the GLD,
				 * at least in DEBUG, so leave it off for now to allow LKE to work in this situation.
				 */
	cli_lex_setup(argc, argv);
	/*      this should be after cli_lex_setup() due to S390 A/E conversion    */
	OPERATOR_LOG_MSG;
	while (1)
	{
		if (!lke_process(argc) || 2 <= argc)
			break;
	}
	lke_exit();
	return 0;
}

static bool lke_process(int argc)
{
	bool		flag = FALSE;
	int		res;
	static int	save_stderr = SYS_STDERR;

	ESTABLISH_RET(util_ch, TRUE);
	if (util_interrupt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
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
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
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
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		} else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
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
>>>>>>> 35326517 (GT.M V7.0-003)
}
