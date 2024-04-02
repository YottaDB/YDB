/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "gtm_signal.h"
#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "iosp.h"
#include "error.h"
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
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "stp_parms.h"
#include "cli.h"
#include "io.h"
#include "mupip_exit.h"
#include "patcode.h"
#include "lke.h"
#include "ydb_chk_dist.h"
#include "ydb_os_signal_handler.h"
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
#include "restrict.h"
#include "dm_audit_log.h"
#include "compiler.h"
#include "send_msg.h"
#include "gtm_unistd.h"
#include "gtm_malloc.h"

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
GBLREF	char			cli_err_str[];
GBLREF	void			(*mupip_exit_fp)(int errcode);
GBLREF	void 			(*primary_exit_handler)(void);
GBLREF	CLI_ENTRY		mupip_cmd_ary[];

int mupip_main(int argc, char **argv, char **envp)
{
	mval		input_line;
	int		res, save_errno;
	int 		i, gtm_dist_len, lenargv, len, cmdlinelen, max_argv;
	boolean_t	is_restricted;
	char 		*cmdletter, *cmdline;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	if (1 < argc)
	{
		cmdletter = argv[1];
		lenargv = STRLEN(argv[1]);
		for (i = 0; i < lenargv; i++)
		{
			if (!IS_ASCII(*cmdletter))
				mupip_exit(ERR_MUPCLIERR); // return an error if the first argument is not ascii
			cmdletter++;
		}
	}
	common_startup_init(MUPIP_IMAGE, &mupip_cmd_ary[0]);
	invocation_mode = MUMPS_UTILTRIGR;
	err_init(util_base_ch);
	DEFINE_EXIT_HANDLER(mupip_exit_handler, TRUE);	/* Must be defined only AFTER err_init() has setup condition handling */
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	sig_init(ydb_os_signal_handler, NULL, suspsigs_handler, continue_handler);	/* Note: no ^C handler is defined (yet) */
	licensed = TRUE;
	in_backup = FALSE;
	op_open_ptr = mu_op_open;
	INIT_FNPTR_GLOBAL_VARIABLES;
	mu_get_term_characterstics();
	ydb_chk_dist(argv[0]);
	is_restricted = RESTRICTED(mupip_enable);
	if (is_restricted)
	{
		errno = 0;
		max_argv = sysconf(_SC_ARG_MAX);
		save_errno = errno;
		if (0 != save_errno)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					RTS_ERROR_LITERAL("sysconf()"), CALLFROM, save_errno);
			mupip_exit(ERR_MUNOACTION);
		}
		errno = 0;
		cmdline = (char *)malloc(max_argv);
		save_errno = errno;
		if (0 != save_errno)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					RTS_ERROR_LITERAL("malloc()"), CALLFROM, save_errno);
			mupip_exit(ERR_MUNOACTION);
		}
		memset(cmdline, '\0', max_argv);
		cmdlinelen = 0;
		if (argc > 0)
			len = SNPRINTF(cmdline, max_argv, "%s ", argv[1]);
		for(i = 2 ; i < argc; i++)
		{
			cmdlinelen = cmdlinelen + len;
			len = SNPRINTF(cmdline + cmdlinelen, max_argv - cmdlinelen, "%s ", argv[i]);
		}
		input_line.mvtype = MV_STR;
		input_line.str.addr = cmdline;
		input_line.str.len = STRLEN(input_line.str.addr);
		if (argc > 1)
		{
			if (!dm_audit_log(&input_line, AUDIT_SRC_MUPIP))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1,
						gtmImageNames[image_type].imageName);
				mupip_exit(ERR_MUNOACTION);
			}
		}
	}
	cli_lex_setup(argc,argv);
	if (is_restricted)
	{
		if (argc < 2)
		{
			SNPRINTF(cmdline, max_argv, "%s ", "MUPIP prompt(restricted)");
			input_line.str.addr = cmdline;
			input_line.str.len = STRLEN(input_line.str.addr);
			dm_audit_log(&input_line, AUDIT_SRC_MUPIP); /* Not checking return value as we are exiting anyway */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1, cmdline);
			mupip_exit(ERR_MUNOACTION);
		}
		free(cmdline);
	}
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
	}
	mupip_exit(SS_NORMAL);
	return 0;
}
