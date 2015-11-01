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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
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
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "cli.h"
#include "cache.h"
#include "gt_timer.h"
#include "io.h"
#include "mupip_exit.h"
#include "getjobnum.h"
#include "patcode.h"
#include "lke.h"
#include "dpgbldir.h"
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "mu_op_open.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "mu_term_setup.h"
#include "sig_init.h"

GBLREF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLDEF	bool			in_backup;
GBLREF	bool			licensed;
GBLREF	bool			transform;
GBLREF	int			(*func)();
GBLREF	mval			curr_gbl_root;
GBLREF	global_latch_t		defer_latch;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	sgm_info         	*first_sgm_info;
GBLREF	cw_set_element   	cw_set[];
GBLREF	unsigned char    	cw_set_depth;
GBLREF	uint4			process_id;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF sgm_info         	*first_sgm_info;
GBLREF cw_set_element   	cw_set[];
GBLREF unsigned char    	cw_set_depth;
GBLREF uint4			process_id;
GBLREF jnlpool_addrs		jnlpool;
GBLREF spdesc   		rts_stringpool, stringpool;

void display_prompt(void);

int main (int argc, char **argv)
{
	int		res;

	image_type = MUPIP_IMAGE;
	err_init(util_base_ch);
	sig_init(generic_signal_handler, NULL);	/* Note: no ^C handler is defined (yet) */
	atexit(mupip_exit_handler);
        SET_LATCH_GLOBAL(&defer_latch, LOCK_AVAILABLE);
	licensed = transform = TRUE;
	in_backup = FALSE;
	op_open_ptr = mu_op_open;
	mu_get_term_characterstics();
	get_page_size();
	getjobnum();
	init_secshr_addrs(get_next_gdr, cw_set, &first_sgm_info, &cw_set_depth, process_id, OS_PAGE_SIZE,
			  &jnlpool.jnlpool_dummy_reg);
	getzdir();
	initialize_pattern_table();
	prealloc_gt_timers();
	cli_lex_setup(argc,argv);
	if (argc < 2)			/* Interactive mode */
		display_prompt();

	/*      this call should be after cli_lex_setup() due to S390 A/E conversion    */
	gtm_chk_dist(argv[0]);
	INIT_GBL_ROOT(); /* Needed for GVT initialization */
	io_init(TRUE);
	cache_init();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	while(1)
	{	func = 0;
		if ((res = parse_cmd()) == EOF)
			break;
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
