/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "iosp.h"
#include "error.h"
#include "cli.h"
#include "stringpool.h"
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
#include "util.h"
#include "cache.h"
#include "gt_timer.h"
#include "lke.h"
#include "lke_fileio.h"
#include "dpgbldir.h"
#include "get_page_size.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "init_secshr_addrs.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "getjobname.h"
#include "getjobnum.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */

GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF enum gtmImageTypes	image_type;
GBLREF sgm_info         	*first_sgm_info;
GBLREF cw_set_element   	cw_set[];
GBLREF unsigned char    	cw_set_depth;
GBLREF uint4			process_id;
GBLREF jnlpool_addrs		jnlpool;
GBLREF char			cli_err_str[];


static bool lke_process(int argc);
static void display_prompt(void);

int main (int argc, char *argv[])
{

	image_type = LKE_IMAGE;
	gtm_env_init();	/* read in all environment variables */
	licensed = TRUE;
	err_init(util_base_ch);
	sig_init(generic_signal_handler, lke_ctrlc_handler);
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
	gvinit();
	region_init(TRUE);
	cache_init();
	getjobnum();

	cli_lex_setup(argc, argv);
	/*      this should be after cli_lex_setup() due to S390 A/E conversion    */
	gtm_chk_dist(argv[0]);

	while (1)
	{
		if (!lke_process(argc))
			break;
	}
	lke_exit();
}

static bool lke_process(int argc)
{
	bool		flag = FALSE;
	int		res;
	static int	save_stderr = 2;

	error_def(ERR_CTRLC);

	ESTABLISH_RET(util_ch, TRUE);
	if (util_interrupt)
		rts_error(VARLSTCNT(1) ERR_CTRLC);
	if (save_stderr != 2)  /* necesary in case of rts_error */
		close_fileio(save_stderr);

	func = 0;
	util_interrupt = 0;
 	if (argc < 2)
		display_prompt();
	if ( EOF == (res = parse_cmd()))
	{
		if (util_interrupt)
		{
			rts_error(VARLSTCNT(1) ERR_CTRLC);
			REVERT;
			return TRUE;
		}
		else
		{
			REVERT;
			return FALSE;
		}
	} else if (res)
	{
		if (1 < argc)
		{
			REVERT;
			rts_error(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
		} else
			gtm_putmsg(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
	}
	if (func)
	{
		flag = open_fileio(&save_stderr); /* save_stderr = 2 if -output option not present */
		func();
		if (flag)
			close_fileio(save_stderr);
	}
	REVERT;
	return(1 >= argc);
}

static void display_prompt(void)
{
	PRINTF("LKE> ");
	fflush(stdout);
}

