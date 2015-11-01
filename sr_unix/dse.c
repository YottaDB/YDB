/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "stp_parms.h"
#include "error.h"
#include "min_max.h"
#include "init_root_gv.h"
#include "interlock.h"
#include "gtmimagename.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "util.h"
#include "cli.h"
#include "cache.h"
#include "op.h"
#include "gt_timer.h"
#include "io.h"
#include "dse.h"
#include "compiler.h"
#include "patcode.h"
#include "lke.h"
#include "dpgbldir.h"
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "dse_exit.h"
#include "getjobname.h"
#include "getjobnum.h"
#include "sig_init.h"

GBLDEF block_id			patch_curr_blk;
GBLREF gd_region		*gv_cur_region;
GBLREF gd_binding		*gd_map;
GBLREF gd_binding		*gd_map_top;
GBLREF gd_addr			*gd_header;
GBLREF gd_addr			*original_header;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF gv_namehead		*gv_target;
GBLREF bool			transform;
GBLREF mval			curr_gbl_root;
GBLREF int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF boolean_t		dse_running;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF enum gtmImageTypes	image_type;
GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF sgm_info         	*first_sgm_info;
GBLREF cw_set_element   	cw_set[];
GBLREF unsigned char    	cw_set_depth;
GBLREF uint4			process_id;
GBLREF jnlpool_addrs		jnlpool;

static bool	dse_process(int argc);
static void display_prompt(void);

int main(int argc, char *argv[])
{
	static char	prompt[]="DSE> ";

	image_type = DSE_IMAGE;
	dse_running = TRUE;
	licensed = TRUE;
	transform = TRUE;
	op_open_ptr = op_open;
	patch_curr_blk = get_dir_root();
	err_init(util_base_ch);
	sig_init(generic_signal_handler, dse_ctrlc_handler);
	atexit(util_exit_handler);
	SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	getjobname();
	init_secshr_addrs(get_next_gdr, cw_set, &first_sgm_info, &cw_set_depth, process_id, OS_PAGE_SIZE,
			  &jnlpool.jnlpool_dummy_reg);
	getzdir();
	prealloc_gt_timers();
	initialize_pattern_table();
	gvinit();
	region_init(FALSE);
	INIT_GBL_ROOT(); /* Needed for GVT initialization */
	cache_init();
	getjobnum();
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	cli_lex_setup(argc, argv);
	CREATE_DUMMY_GBLDIR(gd_header, original_header, gv_cur_region, gd_map, gd_map_top);
	gtm_chk_dist(argv[0]);
	if (argc < 2)
                display_prompt();
	io_init(TRUE);
	while (1)
	{
		if (!dse_process(argc))
			break;
                display_prompt();
	}
	dse_exit();
	REVERT;
}

static void display_prompt(void)
{
	PRINTF("DSE> ");
	fflush(stdout);
}

static bool	dse_process(int argc)
{
	int	res;
	error_def(ERR_CTRLC);

	ESTABLISH_RET(util_ch, TRUE);
	func = 0;
	util_interrupt = 0;
	if (EOF == (res = parse_cmd()))
	{
		if (util_interrupt)
		{
			rts_error(VARLSTCNT(1) ERR_CTRLC);
			REVERT;
			return TRUE;
		} else
		{
			REVERT;
			return FALSE;
		}
	}
	if (func)
		func();

	if (argc > 1)
	{
		REVERT;
		return FALSE;
	}
	REVERT;
	return TRUE;
}
