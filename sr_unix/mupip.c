/****************************************************************
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
=======
#include "io.h"
#include "mupip_exit.h"
#include "patcode.h"
#include "lke.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "mu_op_open.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "mu_term_setup.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "suspsigs_handler.h"
#include "startup.h"
#include "gtm_startup.h"
#include "invocation_mode.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "continue_handler.h"
#include "gtmio.h"

#ifdef UTF8_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF	int			(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF	bool			in_backup;
GBLREF	bool			licensed;
GBLREF	int			(*func)();
GBLREF	global_latch_t		defer_latch;
GBLREF	spdesc			rts_stringpool, stringpool;
GBLREF	char			cli_err_str[];
GBLREF	CLI_ENTRY		mupip_cmd_ary[];
GBLREF	void			(*mupip_exit_fp)(int errcode);

GBLDEF	CLI_ENTRY		*cmd_ary = &mupip_cmd_ary[0];	/* Define cmd_ary to be the MUPIP specific cmd table */

void display_prompt(void);

int main (int argc, char **argv)
{
	int		res;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(MUPIP_IMAGE);
	invocation_mode = MUMPS_UTILTRIGR;
	err_init(util_base_ch);
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	sig_init(generic_signal_handler, NULL, suspsigs_handler, continue_handler);	/* Note: no ^C handler is defined (yet) */
	atexit(mupip_exit_handler);
	licensed = TRUE;
	in_backup = FALSE;
	op_open_ptr = mu_op_open;
	INIT_FNPTR_GLOBAL_VARIABLES;
	mu_get_term_characterstics();
	gtm_chk_dist(argv[0]);
	cli_lex_setup(argc,argv);
	if (argc < 2)			/* Interactive mode */
		display_prompt();
	/*      this call should be after cli_lex_setup() due to S390 A/E conversion    */
	init_gtm();
	mupip_exit_fp = mupip_exit;	/* Initialize function pointer for use during MUPIP */
	while (TRUE)
	{	func = 0;
		if ((res = parse_cmd()) == EOF)
			break;
		else if (res)
		{
			if (1 < argc)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
			else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		}
		if (func)
			func();
		if (argc > 1)		/* Non-interactive mode, exit after command */
			break;
		display_prompt();
	}
	mupip_exit(SS_NORMAL);
	return 0;
}
>>>>>>> 7a1d2b3e... GT.M V6.3-007

int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "mupip_main");
}
