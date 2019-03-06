/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
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
	return dlopen_libyottadb(argc, argv, envp, "lke_main");
=======
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(LKE_IMAGE);
	licensed = TRUE;
	err_init(util_base_ch);
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	sig_init(generic_signal_handler, lke_ctrlc_handler, suspsigs_handler, continue_handler);
	atexit(util_exit_handler);
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
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
>>>>>>> 7a1d2b3e... GT.M V6.3-007
}
