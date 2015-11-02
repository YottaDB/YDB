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
#include <signal.h>

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "iosp.h"
#include "error.h"
#include "min_max.h"
#include "init_root_gv.h"
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
#include "hashtab_int4.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "cli.h"
#include "gt_timer.h"
#include "io.h"
#include "mupip_exit.h"
#include "getjobnum.h"
#include "patcode.h"
#include "lke.h"
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "mu_op_open.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "mu_term_setup.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "suspsigs_handler.h"
#include "startup.h"
#include "gtm_startup.h"
#include "invocation_mode.h"
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"
#include "continue_handler.h"

#ifdef UNICODE_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF	bool			in_backup;
GBLREF	bool			licensed;
GBLREF	int			(*func)();
GBLREF	mval			curr_gbl_root;
GBLREF	global_latch_t		defer_latch;
GBLREF	spdesc			rts_stringpool, stringpool;
GBLREF	char			cli_err_str[];
GBLREF	CLI_ENTRY		mupip_cmd_ary[];

GBLDEF	CLI_ENTRY		*cmd_ary = &mupip_cmd_ary[0];	/* Define cmd_ary to be the MUPIP specific cmd table */

void display_prompt(void);

int main (int argc, char **argv)
{
	int		res;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	set_blocksig();
	gtm_imagetype_init(MUPIP_IMAGE);
	invocation_mode = MUMPS_UTILTRIGR;
	gtm_wcswidth_fnptr = gtm_wcswidth;
	gtm_env_init();	/* read in all environment variables */
	err_init(util_base_ch);
	UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	sig_init(generic_signal_handler, NULL, suspsigs_handler, continue_handler);	/* Note: no ^C handler is defined (yet) */
	atexit(mupip_exit_handler);
	licensed = TRUE;
	in_backup = FALSE;
	op_open_ptr = mu_op_open;
	mu_get_term_characterstics();
	gtm_chk_dist(argv[0]);
	cli_lex_setup(argc,argv);
	if (argc < 2)			/* Interactive mode */
		display_prompt();
	/*      this call should be after cli_lex_setup() due to S390 A/E conversion    */
	INIT_GBL_ROOT(); /* Needed for GVT initialization */
	init_gtm();
	while (TRUE)
	{	func = 0;
		if ((res = parse_cmd()) == EOF)
			break;
		else if (res)
		{
			if (1 < argc)
				rts_error(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
			else
				gtm_putmsg(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		}
		if (func)
			func();
		if (argc > 1)		/* Non-interactive mode, exit after command */
			break;
		display_prompt();
	}
	mupip_exit(SS_NORMAL);
}

void display_prompt(void)
{
	PRINTF("MUPIP> ");
}
