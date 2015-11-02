/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "muextr.h"
#include "iosp.h"
#include "cli.h"
#include "mu_reorg.h"
#include "util.h"
#include "filestruct.h"
#include "error.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"

/* Prototypes */
#include "mupip_size.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "mu_getlst.h"

error_def(ERR_NOSELECT);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNOACTION);
error_def(ERR_MUSIZEINVARG);

GBLREF tp_region	*grlist;
GBLREF bool		error_mupip;
GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;

typedef struct {
	enum {arsample, scan, impsample}	heuristic;
	int4 					samples;
	int4 					level;
	mval					*global_name;
	int4					seed;
} mupip_size_cfg_t;

STATICFNDCL void mupip_size_check_error(void);

/*
 * This function reads command line parameters and forms a configuration for mupip size invocation.
 * It later executes mupip size on each global based on the configuration
 *
 * MUPIP SIZE interface is described in GTM-7292
 */
void mupip_size(void)
{
	uint4			status = EXIT_NRM;
	glist			gl_head, exclude_gl_head, *gl_ptr;
	/* configuration default values */
	mupip_size_cfg_t	mupip_size_cfg = { impsample, 1000, 1, 0, 0 };
	char			cli_buff[MAX_LINE];
	int4			reg_max_rec, reg_max_key, reg_max_blk;
	unsigned short		n_len;
	char 			buff[MAX_LINE];
	unsigned short		BUFF_LEN = SIZEOF(buff);
	char 			*p_end;						/* used for strtol validation */
	boolean_t		restrict_reg = FALSE;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_outofband_setup();
	error_mupip = FALSE;

	/* Region qualifier */
	grlist = NULL;
	if (CLI_PRESENT == cli_present("REGION"))
	{
		restrict_reg = TRUE;
		gvinit();				/* initialize gd_header (needed by the following call to mu_getlst) */
		mu_getlst("REGION", SIZEOF(tp_region));	/* get the parameter corresponding to REGION qualifier */
	}
	mupip_size_check_error();

	/* SELECT qualifier */
	memset(cli_buff, 0, SIZEOF(cli_buff));
	n_len = SIZEOF(cli_buff);
	if (CLI_PRESENT != cli_present("SELECT"))
	{
		n_len = 1;
		cli_buff[0] = '*';
	}
	else if (FALSE == cli_get_str("SELECT", cli_buff, &n_len))
	{
		n_len = 1;
		cli_buff[0] = '*';
	}
	/* gv_select will select globals for this clause*/
	gv_select(cli_buff, n_len, FALSE, "SELECT", &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, restrict_reg);
	if (!gl_head.next){
		error_mupip = TRUE;
		gtm_putmsg(VARLSTCNT(1) ERR_NOSELECT);
	}
	mupip_size_check_error();

	/* HEURISTIC qualifier */
	if (cli_present("HEURISTIC.SCAN") == CLI_PRESENT)
	{
		mupip_size_cfg.heuristic = scan;
		if (cli_present("HEURISTIC.LEVEL"))
		{
			boolean_t valid = TRUE;
			if (cli_get_str("HEURISTIC.LEVEL", buff, &BUFF_LEN))
			{
				mupip_size_cfg.level = strtol(buff, &p_end, 10);
				valid = (*p_end == '\0');
			}
			else
				valid = FALSE;
			if (!valid || mupip_size_cfg.level <= -MAX_BT_DEPTH || MAX_BT_DEPTH <= mupip_size_cfg.level)
			{
				error_mupip = TRUE;
				gtm_putmsg(VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.LEVEL"));
			}
		}
		/* else level is already initialized with default value */
	}
	else if (cli_present("HEURISTIC.ARSAMPLE") == CLI_PRESENT || cli_present("HEURISTIC.IMPSAMPLE") == CLI_PRESENT)
	{
		if (cli_present("HEURISTIC.ARSAMPLE") == CLI_PRESENT)
			mupip_size_cfg.heuristic = arsample;
		else if (cli_present("HEURISTIC.IMPSAMPLE") == CLI_PRESENT)
			mupip_size_cfg.heuristic = impsample;
		if (cli_present("HEURISTIC.SAMPLES"))
		{
			boolean_t valid = cli_get_int("HEURISTIC.SAMPLES", &(mupip_size_cfg.samples));
			if (!valid || mupip_size_cfg.samples <= 0){
				error_mupip = TRUE;
				gtm_putmsg(VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.SAMPLES"));
			}
		}
		/* else samples is already initialized with default value */

		/* undocumented SEED parameter used for testing sampling method */
		if (cli_present("HEURISTIC.SEED"))
		{
			boolean_t valid = cli_get_int("HEURISTIC.SEED", &(mupip_size_cfg.seed));
			if (!valid){
				error_mupip = TRUE;
				gtm_putmsg(VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.SEED"));
			}
		}
		/* else seed will be based on the time */
	}
	mupip_size_check_error();

	/* run mupip size on each global */
	for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		util_out_print("!/Global: !AD ", FLUSH, gl_ptr->name.str.len, gl_ptr->name.str.addr);

		mupip_size_cfg.global_name = &(gl_ptr->name);
		switch (mupip_size_cfg.heuristic)
		{
		case scan:
			status |= mu_size_scan(mupip_size_cfg.global_name, mupip_size_cfg.level);
			break;
		case arsample:
			status |= mu_size_arsample(mupip_size_cfg.global_name, mupip_size_cfg.samples, TRUE, mupip_size_cfg.seed);
			break;
		case impsample:
			status |= mu_size_impsample(mupip_size_cfg.global_name, mupip_size_cfg.samples, mupip_size_cfg.seed);
			break;
		default:
			GTMASSERT;
			break;
		}
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
			mupip_exit(ERR_MUNOFINISH);
	}

	mupip_exit(status ==  EXIT_NRM ? SS_NORMAL : ERR_MUNOFINISH);
}


STATICDEF void mupip_size_check_error(void)
{
	if (error_mupip)
	{
		util_out_print("!/MUPIP SIZE cannot proceed with above errors!/", FLUSH);
		mupip_exit(ERR_MUNOACTION);
	}
}
