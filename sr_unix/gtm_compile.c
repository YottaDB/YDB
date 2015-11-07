/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
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

GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF IN_PARMS			*cli_lex_in_ptr;
GBLREF stack_frame	 	*frame_pointer;
GBLREF unsigned char 		*stackbase,*stacktop,*stackwarn,*msp;
GBLREF mv_stent			*mv_chain;
GBLREF symval			*curr_symval;
GBLREF boolean_t		run_time;
GBLREF spdesc			rts_stringpool, stringpool;

int	gtm_compile (void)
{
	int			status;
	unsigned short		len;
	char			source_file_string[MAX_FBUFF + 1];
	char			obj_file[MAX_FBUFF + 1], list_file[MAX_FBUFF + 1], ceprep_file[MAX_FBUFF + 1];
	unsigned char		*mstack_ptr;
	void 			gtm_ret_code();
	command_qualifier	save_qlf;
	mstr			orig_cmdstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	io_init(TRUE);
	getjobnum();
	getzdir();
	run_time = FALSE;
	TREF(compile_time) = TRUE;
	mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
	msp = stackbase = mstack_ptr + (USER_STACK_SIZE - SIZEOF(char *));
	mv_chain = (mv_stent *)msp;
	stackwarn = stacktop + (USER_STACK_SIZE / 4);
	msp -= SIZEOF(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, SIZEOF(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer;
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer->type = SFT_COUNT;
	frame_pointer->rvector = (rhdtyp *)malloc(SIZEOF(rhdtyp));
	memset(frame_pointer->rvector, 0, SIZEOF(rhdtyp));
	symbinit();
	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	TREF(zsearch_var) = lv_getslot(curr_symval);
	TREF(zsearch_dir1) = lv_getslot(curr_symval);
	TREF(zsearch_dir2) = lv_getslot(curr_symval);
	LVVAL_INIT((TREF(zsearch_var)), curr_symval);
	LVVAL_INIT((TREF(zsearch_dir1)), curr_symval);
	LVVAL_INIT((TREF(zsearch_dir2)), curr_symval);
	/* command qualifier processing stuff */
	zco_init();
	assert(cli_lex_in_ptr);
	save_qlf = glb_cmd_qlf;
	/* Save the original MUMPS command line qualifers. These are processed after, and take precedence over, $ZCOMPILE. */
	orig_cmdstr.len = strlen(cli_lex_in_ptr->in_str) - strlen("MUMPS ");
	orig_cmdstr.addr = (char *)malloc(orig_cmdstr.len);
	memcpy(orig_cmdstr.addr, cli_lex_in_ptr->in_str + strlen("MUMPS "), orig_cmdstr.len);
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, MAX_FBUFF);
	zl_cmd_qlf(&(TREF(dollar_zcompile)), &cmd_qlf);	/* Initialize $ZCOMPILE as default command qualifiers */
	glb_cmd_qlf = cmd_qlf;
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, MAX_FBUFF);
	zl_cmd_qlf(&orig_cmdstr, &cmd_qlf);		/* Next, process MUMPS command line qualifers */
	free(orig_cmdstr.addr);
	glb_cmd_qlf = save_qlf;
	/* end command qualifier processing stuff */
	initialize_pattern_table();
	ce_init();	/* initialize compiler escape processing */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();	/* Not sure why compiler needs timers but .. */
	TREF(dollar_zcstatus) = SS_NORMAL;
	len = MAX_FBUFF;
	for (status = cli_get_str("INFILE", source_file_string, &len);
		status;
		status = cli_get_str("INFILE", source_file_string, &len))
	{
		compile_source_file(len, source_file_string, TRUE);
		len = MAX_FBUFF;
	}
	print_exit_stats();
	SET_PROCESS_EXITING_TRUE;	/* needed by remove_rms($principal) to avoid closing that */
	io_rundown(NORMAL_RUNDOWN);
	return (SS_NORMAL == TREF(dollar_zcstatus)) ? SS_NORMAL : 1;
}
