/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "iosp.h"
#include "cli.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "lv_val.h"
#include "parse_file.h"
#include "source_file.h"
#include "gt_timer.h"
#include "io.h"
#include "getjobnum.h"
#include "comp_esc.h"
#include "get_page_size.h"
#include "getzdir.h"
#include "gtm_compile.h"
#include "patcode.h"
#include "print_exit_stats.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "zco_init.h"
#include "min_max.h"

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;
GBLREF IN_PARMS		*cli_lex_in_ptr;
GBLREF mident			module_name;
GBLREF mv_stent		*mv_chain;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF stack_frame		*frame_pointer;
GBLREF symval			*curr_symval;
GBLREF unsigned char 		*msp, *stackbase, *stacktop, *stackwarn;

int	gtm_compile(void)
{
	boolean_t		more;
	char			ceprep_file[MAX_FN_LEN + 1], list_file[MAX_FN_LEN + 1], obj_file[MAX_FN_LEN + 1];
	char			source_file_string[MAX_FN_LEN + 1];
	int			cum_status, status;
	mstr			orig_cmdstr;
	unsigned char		*mstack_ptr;
	unsigned short		len;
	size_t			origlen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	io_init(TRUE);
	getjobnum();
	getzdir();
	run_time = TRUE;		/* This and the next seem odd, but called routines expect it */
	TREF(compile_time) = FALSE;
	mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
	msp = stackbase = mstack_ptr + (USER_STACK_SIZE - sizeof(char *));
	mv_chain = (mv_stent *)msp;
	stackwarn = stacktop + (USER_STACK_SIZE / 4);
	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer;
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer->type = SFT_COUNT;
	frame_pointer->rvector = (rhdtyp *)malloc(sizeof(rhdtyp));
	memset(frame_pointer->rvector, 0, sizeof(rhdtyp));
	symbinit();
	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	TREF(zsearch_var) = lv_getslot(curr_symval);
	LVVAL_INIT((TREF(zsearch_var)), curr_symval);
	/* command qualifier processing stuff */
	zco_init();
	assert(cli_lex_in_ptr);
	/* Save the original MUMPS command line qualifers, which starts with "MUMPS ". These are processed after,
	 * and take precedence over, $ZCOMPILE. */
	origlen = strlen(cli_lex_in_ptr->in_str);
	assert(STR_LIT_LEN("MUMPS ") <= origlen);
	origlen -= STR_LIT_LEN("MUMPS ");
	assert(MAXPOSINT4 >= origlen);
	orig_cmdstr.addr = (char *)malloc(origlen);
	memcpy(orig_cmdstr.addr, (char *)(cli_lex_in_ptr->in_str + (STR_LIT_LEN("MUMPS "))), origlen);
	orig_cmdstr.len = (int)origlen;
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, MAX_FN_LEN);
	len = module_name.len = 0;
	zl_cmd_qlf(&(TREF(dollar_zcompile)), &cmd_qlf, source_file_string, &len, FALSE);	/* Init with default quals */
	zl_cmd_qlf(&orig_cmdstr, &cmd_qlf, source_file_string, &len, TRUE);		/* Override with the actual qualifers */
	free(orig_cmdstr.addr);
	/* end command qualifier processing stuff */
	ce_init();	/* initialize compiler escape processing */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();	/* Not sure why compiler needs timers but .. */
	cum_status = SS_NORMAL;
	do {
		compile_source_file(len, source_file_string, TRUE);
		cum_status |= TREF(dollar_zcstatus);
		cmd_qlf.object_file.str.len = module_name.len = 0;
		len = MAX_FN_LEN;
		status = cli_get_str("INFILE", source_file_string, &len);
	} while (status);
	print_exit_stats();
	SET_PROCESS_EXITING_TRUE;	/* needed by remove_rms($principal) to avoid closing that */
	io_rundown(NORMAL_RUNDOWN);
	return (SS_NORMAL == cum_status) ? SS_NORMAL : 1;
}
