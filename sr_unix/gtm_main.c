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

#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "startup.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "error.h"
#include "cli.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtmimagename.h"
#include "op.h"
#include "tp_timeout.h"
#include "ctrlc_handler.h"
#include "gtm_startup_chk.h"
#include "gtm_startup.h"
#include "jobchild_init.h"
#include "cli_parse.h"
#include "invocation_mode.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_main.h"		/* for "gtm_main" prototype */
#include "io.h"
#include "gt_timer.h"
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif
#ifdef GTM_CRYPT
#include "gtmci.h"
#include "gtmcrypt.h"
#define GTM_PASSWD "gtm_passwd"
#endif

GBLREF	IN_PARMS			*cli_lex_in_ptr;
GBLREF	char				cli_token_buf[];
GBLREF	char				cli_err_str[];
GBLREF	CLI_ENTRY			mumps_cmd_ary[];
GTMTRIG_DBG_ONLY(GBLREF	ch_ret_type	(*ch_at_trigger_init)();)
#ifdef UNICODE_SUPPORTED
GBLREF	u_casemap_t 			gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLDEF	CLI_ENTRY			*cmd_ary = &mumps_cmd_ary[0]; /* Define cmd_ary to be the MUMPS specific cmd table */
GBLREF	boolean_t			skip_dbtriggers;

#ifdef __osf__
	/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
	 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
	 * the only exception and, in particular because the operating system does not support such an exception, the argv
	 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
	 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
	 */
# pragma pointer_size (save)
# pragma pointer_size (long)
#endif
GBLDEF char 		**gtmenvp;
#ifdef GTM_CRYPT
error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTINIT2);
#endif
int gtm_main (int argc, char **argv, char **envp)
#ifdef __osf__
# pragma pointer_size (restore)
#endif
{
	char			*ptr;
	int             	eof, parse_ret;
#	ifdef GTM_CRYPT
	char			*gtm_passwd;
	const char		*gtmcrypt_errlit = "during GT.M startup";
	int			gtmcrypt_errno;
#	endif
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	set_blocksig();
	gtmenvp = envp;
	gtm_imagetype_init(GTM_IMAGE);
	GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mdb_condition_handler);
	gtm_wcswidth_fnptr = gtm_wcswidth;
	gtm_env_init();	/* read in all environment variables */
	err_init(stop_image_conditional_core);
	UNICODE_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	GTM_ICU_INIT_IF_NEEDED;	/* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	cli_lex_setup(argc, argv);
	/* put the arguments into buffer, then clean up the token buffer
	 * cli_gettoken() copies all arguments except the first one argv[0]
	 * into the buffer (cli_lex_in_ptr->in_str).
	 * i.e. command line: "/usr/library/V990/mumps -run somefile"
	 * the buffer cli_lex_in_ptr->in_str == "-run somefile"
	 */
	if (1 < argc)
		cli_gettoken(&eof);
	/* cli_gettoken() extracts the first token into cli_token_buf (in tok_extract())
	 * which should be done in parse_cmd(), So, reset the token buffer here to make
	 * parse_cmd() starts from the first token
	*/
	cli_token_buf[0] = '\0';
	/* insert the "MUMPS " in the parsing buffer the buffer is now:
	 * cli_lex_in_ptr->in_str == "MUMPS -run somefile"
	 * we didnot change argv[0]
	*/
	ptr = cli_lex_in_ptr->in_str;
	memmove(strlen("MUMPS ") + ptr, ptr, strlen(ptr) + 1);	/* BYPASSOK */
	MEMCPY_LIT(ptr, "MUMPS ");
	/* reset the argument buffer pointer, it's changed in cli_gettoken() call above
	 * do NOT reset to 0(NULL) to avoid fetching cmd line args into buffer again
	 * cli_lex_in_ptr->tp is the pointer to indicate current position in the buffer
	 * cli_lex_in_ptr->in_str
	 */
	cli_lex_in_ptr->tp = cli_lex_in_ptr->in_str;
	parse_ret = parse_cmd();
	if (parse_ret && (EOF != parse_ret))
		rts_error(VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));
	if (cli_present("DIRECT_MODE"))
		invocation_mode = MUMPS_DIRECT;
	else if (cli_present("RUN"))
		invocation_mode = MUMPS_RUN;
	gtm_chk_dist(argv[0]);
	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	init_gtm();
#	ifdef GTM_CRYPT
	if (MUMPS_COMPILE != invocation_mode
	    && (NULL != (gtm_passwd = (char *)getenv(GTM_PASSWD)))
	    && (0 == strlen(gtm_passwd)))
	{
		INIT_PROC_ENCRYPTION(NULL, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			CLEAR_CRYPTERR_MASK(gtmcrypt_errno);
			assert(!IS_REPEAT_MSG_MASK(gtmcrypt_errno));
			assert((ERR_CRYPTDLNOOPEN == gtmcrypt_errno) || (ERR_CRYPTINIT == gtmcrypt_errno));
			if (ERR_CRYPTDLNOOPEN == gtmcrypt_errno)
				gtmcrypt_errno = ERR_CRYPTDLNOOPEN2;
			else if (ERR_CRYPTINIT == gtmcrypt_errno)
				gtmcrypt_errno = ERR_CRYPTINIT2;
			gtmcrypt_errno = SET_CRYPTERR_MASK(gtmcrypt_errno);
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, STRLEN(gtmcrypt_errlit), gtmcrypt_errlit);
		}
	}
#	endif
	dm_start();
	return 0;
}
