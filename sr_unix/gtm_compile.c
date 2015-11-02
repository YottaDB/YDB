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

#include "gtm_string.h"

#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "iosp.h"
#include "cli.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
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

GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF stack_frame	 	*frame_pointer;
GBLREF unsigned char 		*stackbase,*stacktop,*stackwarn,*msp;
GBLREF mv_stent			*mv_chain;
GBLREF lv_val			*zsrch_var, *zsrch_dir1, *zsrch_dir2;
GBLREF symval			*curr_symval;
GBLREF bool			run_time, compile_time;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF int4			dollar_zcstatus;

int	gtm_compile (void)
{
	int		status;
	unsigned short	len;
	char		source_file_string[MAX_FBUFF + 1];
	char		obj_file[MAX_FBUFF + 1], list_file[MAX_FBUFF + 1], ceprep_file[MAX_FBUFF + 1];
	unsigned char	*mstack_ptr;
	void 		gtm_ret_code();

	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	io_init(TRUE);
	getjobnum();
	getzdir();
	prealloc_gt_timers();
	run_time = FALSE;
	compile_time = TRUE;
        mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
        msp = stackbase = mstack_ptr + (USER_STACK_SIZE - sizeof(char *));
	mv_chain = (mv_stent *) msp;
        stackwarn = stacktop + (USER_STACK_SIZE / 4);

	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *) msp;
	memset(frame_pointer,0, sizeof(stack_frame));
	frame_pointer->temps_ptr = (unsigned char *) frame_pointer;
	frame_pointer->mpc = CODE_ADDRESS(gtm_ret_code);
	frame_pointer->ctxt = GTM_CONTEXT(gtm_ret_code);
	frame_pointer->type = SFT_COUNT;
	frame_pointer->rvector = (rhdtyp*)malloc(sizeof(rhdtyp));
	memset(frame_pointer->rvector,0,sizeof(rhdtyp));
	symbinit();

	/* Variables for supporting $ZSEARCH sorting and wildcard expansion */
	zsrch_var = lv_getslot(curr_symval);
	zsrch_dir1 = lv_getslot(curr_symval);
	zsrch_dir2 = lv_getslot(curr_symval);
	zsrch_var->v.mvtype = zsrch_dir1->v.mvtype = zsrch_dir2->v.mvtype = 0;
	zsrch_var->tp_var = zsrch_dir1->tp_var = zsrch_dir2->tp_var = 0;
	zsrch_var->ptrs.val_ent.children = zsrch_dir1->ptrs.val_ent.children =
		zsrch_dir2->ptrs.val_ent.children = 0;
	zsrch_var->ptrs.val_ent.parent.sym = zsrch_dir1->ptrs.val_ent.parent.sym =
		zsrch_dir2->ptrs.val_ent.parent.sym = curr_symval;

	cmd_qlf.object_file.str.addr = obj_file;
	cmd_qlf.object_file.str.len = MAX_FBUFF;
	cmd_qlf.list_file.str.addr = list_file;
	cmd_qlf.list_file.str.len = MAX_FBUFF;
	cmd_qlf.ceprep_file.str.addr = ceprep_file;
	cmd_qlf.ceprep_file.str.len = MAX_FBUFF;
	get_cmd_qlf(&cmd_qlf);
	initialize_pattern_table();
	ce_init();	/* initialize compiler escape processing */

	dollar_zcstatus = 1;
	len = MAX_FBUFF;
	for (status = cli_get_str("INFILE",source_file_string,&len);
		status;
		status = cli_get_str("INFILE",source_file_string, &len))
	{
		compile_source_file(len, source_file_string);
		len = MAX_FBUFF;
	}
	print_exit_stats();
	io_rundown(NORMAL_RUNDOWN);
	if (!(dollar_zcstatus & 1))
		return -1;
	else
		return SS_NORMAL;
}
