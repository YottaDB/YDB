/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
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
#include "mprof.h"
#include "gtm_startup_chk.h"
#include "gtm_compile.h"
#include "gtm_startup.h"
#include "jobchild_init.h"
#include "cli_parse.h"
#include "invocation_mode.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_main.h"		/* for "gtm_main" prototype */

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF enum gtmImageTypes	image_type;
GBLREF IN_PARMS			*cli_lex_in_ptr;
GBLREF char			cli_token_buf[];
GBLREF char			cli_err_str[];
GBLREF boolean_t		gtm_utf8_mode;

#ifdef __osf__
	/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
	 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
	 * the only exception and, in particular because the operating system does not support such an exception, the argv
	 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
	 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
	 */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

GBLDEF char 		**gtmenvp;

int gtm_main (int argc, char **argv, char **envp)

#ifdef __osf__
#pragma pointer_size (restore)
#endif

{
	char		*ptr;
	int             eof, parse_ret;

	gtmenvp = envp;
	image_type = GTM_IMAGE;
	gtm_wcswidth_fnptr = gtm_wcswidth;
	gtm_env_init();	/* read in all environment variables */
	err_init(stop_image_conditional_core);
	if (gtm_utf8_mode)
		gtm_icu_init();	 /* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	cli_lex_setup(argc, argv);
	/*	put the arguments into buffer, then clean up the token buffer
		cli_gettoken() copies all arguments except the first one argv[0]
		into the buffer (cli_lex_in_ptr->in_str).
		i.e. command line: "/usr/library/V990/mumps -run somefile"
		the buffer cli_lex_in_ptr->in_str == "-run somefile"	*/
	if (1 < argc)
		cli_gettoken(&eof);
	/*	cli_gettoken() extracts the first token into cli_token_buf (in tok_extract())
		which should be done in parse_cmd(), So, reset the token buffer here to make
		parse_cmd() starts from the first token
	*/
	cli_token_buf[0] = '\0';
	/*	insert the "MUMPS " in the parsing buffer the buffer is now:
		cli_lex_in_ptr->in_str == "MUMPS -run somefile"
		we didnot change argv[0]
	*/
	ptr = cli_lex_in_ptr->in_str;
	memmove(strlen("MUMPS ") + ptr, ptr, strlen(ptr) + 1);
	MEMCPY_LIT(ptr, "MUMPS ");

	/*	reset the argument buffer pointer, it's changed in cli_gettoken() call above    */
	/*	do NOT reset to 0(NULL) to avoid fetching cmd line args into buffer again       */
	/*	cli_lex_in_ptr->tp is the pointer to indicate current position in the buffer    */
	/*	cli_lex_in_ptr->in_str                                                          */
	cli_lex_in_ptr->tp = cli_lex_in_ptr->in_str;
	parse_ret = parse_cmd();
	if (parse_ret && (EOF != parse_ret))
		rts_error(VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));

	if (cli_present("DIRECT_MODE"))
		invocation_mode = MUMPS_DIRECT;
	else if (cli_present("RUN"))
		invocation_mode = MUMPS_RUN;

	gtm_chk_dist(argv[0]);
	gtm_chk_image();
	/* this should be after cli_lex_setup() due to S390 A/E conversion in cli_lex_setup   */
	init_gtm();
	dm_start();
	return 0;
}
