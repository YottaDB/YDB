/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "ydb_chk_dist.h"
#include "gtm_startup.h"
#include "jobchild_init.h"
#include "cli_parse.h"
#include "invocation_mode.h"
#include "io.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "job.h"
#include "restrict.h"
#include "release_name.h"

#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif
#include "gtmcrypt.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#include "ydb_getenv.h"
#endif
#include "ydb_shebang.h"

GBLREF	IN_PARMS			*cli_lex_in_ptr;
GBLREF	char				cli_token_buf[];
GBLREF	char				cli_err_str[];
GBLREF	boolean_t			ydb_dist_ok_to_use;
GBLREF	char				ydb_dist[YDB_PATH_MAX];
GBLREF	CLI_ENTRY			mumps_cmd_ary[];
GBLREF	boolean_t			skip_dbtriggers;
GBLREF	boolean_t			noThreadAPI_active;
GBLREF	boolean_t			simpleThreadAPI_active;
#if defined (GTM_TRIGGER) && (DEBUG)
GBLREF	ch_ret_type			(*ch_at_trigger_init)();
#endif
#ifdef UTF8_SUPPORTED
GBLREF	u_casemap_t 			gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif
GBLREF	char 				**gtmenvp;
GBLREF	boolean_t			shebang_invocation;	/* TRUE if yottadb is invoked through the "ydbsh" soft link */

#define GTMCRYPT_ERRLIT			"during GT.M startup"
#define YDBXC_gblstat			"%s/gtmgblstat.xc"
#define YDBXC_gtmtlsfuncs		"%s/plugin/gtmtlsfuncs.tab"
#define	MUMPS				"MUMPS "

error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTINIT2);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TEXT);
error_def(ERR_TLSDLLNOOPEN);
error_def(ERR_TLSINIT);

int gtm_main(int argc, char **argv, char **envp)
{
	char		*ptr, *eq, **p;
	char		gtmlibxc[YDB_PATH_MAX], gtmtlsfuncs[YDB_PATH_MAX];
	int		eof, parse_ret, cli_ret;
	int		gtmcrypt_errno;
	int		status;
	char		*pathptr;				/* this is similar to code in "dlopen_libyottadb.c" */
	char		curr_exe_realpath[YDB_PATH_MAX];	/* this is similar to code in "dlopen_libyottadb.c" */
	size_t		cplen;
	char		*exe_basename;

#	ifdef GTM_SOCKET_SSL_SUPPORT
	char			tlsid_env_name[MAX_TLSID_LEN * 2];
#	endif
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	gtmenvp = envp;
	common_startup_init(GTM_IMAGE, &mumps_cmd_ary[0]);
	GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mdb_condition_handler);
	err_init(stop_image_conditional_core);
	assert(!(noThreadAPI_active || simpleThreadAPI_active));	/* Neither should be set unless we recursed (never!) */
	noThreadAPI_active = TRUE;
	UTF8_ONLY(gtm_strToTitle_ptr = &gtm_strToTitle);
	pathptr = realpath(PROCSELF, curr_exe_realpath);	/* this is similar to code in "dlopen_libyottadb.c" */
	assert(NULL != pathptr);	/* or else "dlopen_libyottadb" would have failed in a similar check */
	ydb_chk_dist(pathptr);
	cli_ret = cli_lex_setup(argc, argv);
	if (cli_ret)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) cli_ret, 2, LEN_AND_STR(cli_err_str));
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
	 * we did not change argv[0]
	*/
	ptr = cli_lex_in_ptr->in_str;
	assert((NULL != ptr) && (cli_lex_in_ptr->buflen > (strlen(ptr) + sizeof(MUMPS))));
	memmove(strlen(MUMPS) + ptr, ptr, strlen(ptr) + 1);	/* BYPASSOK */
	MEMCPY_LIT(ptr, MUMPS);
	/* reset the argument buffer pointer, it's changed in cli_gettoken() call above
	 * do NOT reset to 0(NULL) to avoid fetching cmd line args into buffer again
	 * cli_lex_in_ptr->tp is the pointer to indicate current position in the buffer
	 * cli_lex_in_ptr->in_str
	 */
	cli_lex_in_ptr->tp = cli_lex_in_ptr->in_str;
	parse_ret = parse_cmd();
	if (parse_ret && (EOF != parse_ret))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));
	invocation_exe_str = argv[0];
	/* Now that we know which program was used in the invocation, check if it is a shebang invocation. */
	exe_basename = strrchr(invocation_exe_str, '/');
	if (NULL == exe_basename)
		exe_basename = invocation_exe_str;
	else
		exe_basename++;
	shebang_invocation = !STRCMP(exe_basename, YDBSH);
	if (cli_present("HELP")) {
		puts("YottaDB: NoSQL database.\n");
		puts("Usage:");
		puts("  yottadb [--option][=<argument>] [<file>]\n");
		puts("Options:");
		puts("  -dir                --direct_mode           Run in direct mode, i.e. interactively.");
		puts("  -[no]dy             --[no]dynamic_literals  Dynamically load and unload certain literal data structures from generated object code.");
		puts("  -[no]e              --[no]embed_source      Embed routine source code in generated object code.");
		puts("  -h                  --help                  Show command line usage.");
		puts("  -[no]ig             --[no]ignore            Optionally specify whether to produce an object file when the compiler detects errors in the source code. The default is to produce an object file, i.e. --ignore.");
		puts("  -[no]in             --[no]inline_literals   Compile routines to use library code in order to load literals instead of generating in-line code to reduce routine size.");
		puts("  -la=<case>          --labels=<case>         Enables (case='LOWER') or disables (case='UPPER') case sensitivity for labels within routines.");
		puts("  -le=<lines>         --length=<lines>        Controls the page length of the listing file.");
		puts("  -[no]lin            --[no]line_entry        Omit per-line M label offset details to decrease total code size at the cost of possibly increasing code execution time.");
		puts("  -[no]lis=<filename> --[no]list=<filename>   Produce a source program listing file, optionally specifying a name for the listing file.");
		puts("  -m                  --machine_code          Produce a source program listing file that contains the generated bytecode and machine code for the program.");
		puts("  -n=<rtnname>        --nameofrtn=<rtnname>   Specify the routine name (default is M program name minus extension).");
		puts("  -[no]o=<filename>   --[no]object=<filename> Produce an output object file, optionally specifying a name for the object file using the optional filename argument.");
		puts("  -r                  --run                   Invoke YottaDB and run the provided entryref.");
		puts("  -s=<lines>          --space=<lines>         Controls the spacing of the output in the listing file.");
		puts("  -v                  --version               Print the current YottaDB version details.");
		puts("  -[no]w              --[no]warnings          Allow or suppress error and warning output. The default is to allow, i.e. --warnings.");
		EXIT(0);
	}
	if (cli_present("VERSION")) {
		char stamp[SIZEOF(YDB_RELEASE_STAMP)];
		char *date, *time, *commit, *saveptr;

		puts("YottaDB release:         " YDB_ZYRELEASE);
		puts("Upstream base version:   GT.M " GTM_ZVERSION);
		puts("Platform:                " YDB_PLATFORM);
		// The release stamp has the format "DATE TIME COMMIT"
		// strtok requires that the string be writable
		memcpy(stamp, YDB_RELEASE_STAMP, SIZEOF(YDB_RELEASE_STAMP));

		date = STRTOK_R(stamp, " ", &saveptr);
		assert(NULL != date);
		time = STRTOK_R(NULL, " ", &saveptr);
		assert(NULL != time);
		// everything until the end of the string
		commit = STRTOK_R(NULL, "\0", &saveptr);
		assert(NULL != commit);
		// dates have the format YYYYMMDD
		printf("Build date/time:         %.4s-%.2s-%.2s %s\n", date, date + 4, date + 6, time);
		printf("Build commit SHA:        %s\n", commit);
		puts("Compiler:                " YDB_COMPILER);
		puts("Compiler Version:        " YDB_COMPILER_VERSION);
		puts("Build Type:              " YDB_BUILD_TYPE);
#		ifdef YDB_ASAN
		puts("ASAN:                    " YDB_ASAN);
#		endif

		EXIT(0);
	}
	if (cli_present("DIRECT_MODE"))
	{
		shebang_invocation = FALSE;
		if (!((ptr = getenv(CHILD_FLAG_ENV)) && strlen(ptr)) && (RESTRICTED(dmode))) /* note assignment */
		{	/* first tell them it's a no-no without engaging the condition handling so we keep control */
			dec_err(VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1, "mumps -direct");
			stop_image_no_core();		/* then kill them off without further delay */
		}
		invocation_mode = MUMPS_DIRECT;
	}
	else if (cli_present("RUN") || shebang_invocation)
		invocation_mode = MUMPS_RUN;
	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	init_gtm();
	SNPRINTF(gtmlibxc, YDB_PATH_MAX, YDBXC_gblstat, ydb_dist);
	SETENV(status, "ydb_xc_gblstat", gtmlibxc);
#	ifdef GTM_TLS
	SNPRINTF(gtmtlsfuncs, YDB_PATH_MAX, YDBXC_gtmtlsfuncs, ydb_dist);
	SETENV(status, "ydb_xc_gtmtlsfuncs", gtmtlsfuncs);
	if (MUMPS_COMPILE != invocation_mode)
	{
		if ((NULL != (ptr = ydb_getenv(YDBENVINDX_PASSWD, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
			&& (0 == strlen(ptr)))
		{
			INIT_PROC_ENCRYPTION(gtmcrypt_errno);
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
		if (NULL != ydb_getenv(YDBENVINDX_CRYPT_CONFIG, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH))
		{	/* Environment is configured for SSL/TLS (and/or encryption). Check if any environment variable of the form
			 * `ydb_tls_passwd_* or gtmtls_passwd_*' is set to NULL string. If so, nudge the SSL/TLS library to read
			 * password(s) from the user.
			 */
			for (p = envp; *p; p++)
			{
				ptr = *p;
				assert(FALSE);	/* When this assert fails, the below GTMTLS_PASSWD_ENV_PREFIX usage needs to be
						 * fixed to use "ydb_getenv" instead of a hardcoded name.
						 */
				if (0 == MEMCMP_LIT(ptr, GTMTLS_PASSWD_ENV_PREFIX))
				{	/* At least one environment variable of $ydb_tls_passwd_* or $gtmtls_passwd_* is found. */
					eq = strchr(ptr, '=');
					if (0 != strlen(eq + 1))
						break; /* Set to non-empty string. No need to initialize the library now. */
					/* Set to empty string. */
					if (NULL == tls_ctx)
					{
						if (SS_NORMAL != (status = gtm_tls_loadlibrary()))
						{
							RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSDLLNOOPEN, 0,
								ERR_TEXT, 2, LEN_AND_STR(dl_err));
						}
						if (NULL == (tls_ctx = gtm_tls_init(GTM_TLS_API_VERSION,
											GTMTLS_OP_INTERACTIVE_MODE)))
						{
							RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSINIT, 0,
								ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error(NULL)));
						}
					}
					assert(NULL != tls_ctx);
					cplen = (eq - ptr);
					if (sizeof(tlsid_env_name) > (cplen + 1))	/* BYPASSOK unsigned comparison */
						cplen = sizeof(tlsid_env_name) - 1;
					memcpy(tlsid_env_name, ptr, cplen);
					tlsid_env_name[cplen] = '\0';
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
