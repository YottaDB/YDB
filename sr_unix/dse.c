/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

int main(int argc, char **argv, char **envp)
{
<<<<<<< HEAD
	return dlopen_libyottadb(argc, argv, envp, "dse_main");
=======
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
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
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
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "DSE");
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
>>>>>>> 451ab477 (GT.M V7.0-000)
}
