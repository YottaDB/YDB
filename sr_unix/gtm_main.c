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
#include "gtm_main.h"		/* for "gtm_main" prototype */
#include "io.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "job.h"
#include "restrict.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif
#include "gtmcrypt.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif

GBLREF	IN_PARMS			*cli_lex_in_ptr;
GBLREF	char				cli_token_buf[];
GBLREF	char				cli_err_str[];
GBLREF	boolean_t			gtm_dist_ok_to_use;
GBLREF	char				gtm_dist[GTM_PATH_MAX];
GBLREF	CLI_ENTRY			mumps_cmd_ary[];
GBLREF	boolean_t			skip_dbtriggers;
#if defined (GTM_TRIGGER) && (DEBUG)
GBLREF	ch_ret_type			(*ch_at_trigger_init)();
#endif
#ifdef UNICODE_SUPPORTED
GBLREF	u_casemap_t 			gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLDEF	CLI_ENTRY			*cmd_ary = &mumps_cmd_ary[0]; /* Define cmd_ary to be the MUMPS specific cmd table */

#define GTMCRYPT_ERRLIT			"during GT.M startup"
#define GTMXC_gblstat			"GTMXC_gblstat=%s/gtmgblstat.xc"

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

GBLDEF	char 				**gtmenvp;

error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTINIT2);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TEXT);
error_def(ERR_TLSDLLNOOPEN);
error_def(ERR_TLSINIT);

int gtm_main (int argc, char **argv, char **envp)
#ifdef __osf__
# pragma pointer_size (restore)
#endif
{
	char			*ptr, *eq, **p;
	char			gtmlibxc[GTM_PATH_MAX];
	int             	eof, parse_ret;
	int			gtmcrypt_errno;
	int			status;

#	ifdef GTM_SOCKET_SSL_SUPPORT
	char			tlsid_env_name[MAX_TLSID_LEN * 2];
#	endif
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	gtmenvp = envp;
	gtm_dist_ok_to_use = TRUE;
	common_startup_init(GTM_IMAGE);
	GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mdb_condition_handler);
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
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));
	if (cli_present("DIRECT_MODE"))
	{
		if (!((ptr = GETENV(CHILD_FLAG_ENV)) && strlen(ptr)) && (RESTRICTED(dmode))) /* note assignment */
		{	/* first tell them it's a no-no without engaging the condition handling so we keep control */
			dec_err(VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1, "mumps -direct");
			stop_image_no_core();		/* then kill them off without further delay */
		}
		invocation_mode = MUMPS_DIRECT;
	}
	else if (cli_present("RUN"))
		invocation_mode = MUMPS_RUN;
	gtm_chk_dist(argv[0]);
	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	init_gtm();
	SNPRINTF(gtmlibxc, GTM_PATH_MAX, GTMXC_gblstat, gtm_dist);
	PUTENV(status, gtmlibxc);
#	ifdef GTM_TLS
	if (MUMPS_COMPILE != invocation_mode)
	{
		if ((NULL != (ptr = (char *)getenv(GTM_PASSWD_ENV))) && (0 == strlen(ptr)))
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
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, SIZEOF(GTMCRYPT_ERRLIT) - 1,
						      GTMCRYPT_ERRLIT);
			}
		}
#		ifdef GTM_SOCKET_SSL_SUPPORT
		/* The below logic is for prefetching the password for TLS identifiers that may have been set in the environment.
		 * But, since SSL support for Socket devices is not yet implemented, this logic need not be enabled as of this
		 * writing. When SSL support for socket devices is implemented, the surrounding #ifdef can be removed.
		 */
		if (NULL != getenv("gtmcrypt_config"))
		{	/* Environment is configured for SSL/TLS (and/or encryption). Check if any environment variable of the form
			 * `gtmtls_passwd_*' is set to NULL string. If so, nudge the SSL/TLS library to read password(s) from the
			 * user.
			 */
			for (p = envp; *p; p++)
			{
				ptr = *p;
				if (0 == MEMCMP_LIT(ptr, GTMTLS_PASSWD_ENV_PREFIX))
				{	/* At least one environment variable of $gtmtls_passwd_* is found. */
					eq = strchr(ptr, '=');
					if (0 != strlen(eq + 1))
						break; /* Set to non-empty string. No need to initialize the library now. */
					/* Set to empty string. */
					if (NULL == tls_ctx)
					{
						if (SS_NORMAL != (status = gtm_tls_loadlibrary()))
						{
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSDLLNOOPEN, 0,
									ERR_TEXT, 2, LEN_AND_STR(dl_err));
						}
						if (NULL == (tls_ctx = gtm_tls_init(GTM_TLS_API_VERSION,
											GTMTLS_OP_INTERACTIVE_MODE)))
						{
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSINIT, 0,
									ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));
						}
					}
					assert(NULL != tls_ctx);
					assert((MAX_TLSID_LEN * 2) > (int)(eq - ptr));
					memcpy(tlsid_env_name, ptr, (int)(eq - ptr));
					tlsid_env_name[(int)(eq - ptr)] = '\0';
					gtm_tls_prefetch_passwd(tls_ctx, tlsid_env_name);
				}
			}
		}
#		endif
	}
#	endif
	dm_start();
	return 0;
}
