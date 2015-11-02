/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_reorg.c:
 *	Main program for mupip reorg (portable) .
 *
 *	This program creates a list of globals to be organized from /SELECT option and then calls mu_reorg() to
 *	reorganize each global seperately but excludes some variables' organization given in /EXCLUDE list.
 *
 *	This alternatively invokes mu_reorg_upgrd_dwngrd in case a MUPIP REORG -UPGRADE or -DOWNGRADE was specified
 */

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
#ifdef VMS
#include <rms.h>		/* needed for muextr.h */
#endif
#include "muextr.h"
#include "iosp.h"
#include "cli.h"
#include "mu_reorg.h"
#include "util.h"

/* Prototypes */
#include "mupip_reorg.h"
#include "mu_reorg_upgrd_dwngrd.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"

GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF boolean_t	mu_reorg_process;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key           *gv_currkey_next_reorg;
GBLREF gv_namehead	*reorg_gv_target;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;

void mupip_reorg(void)
{
	boolean_t		resume, reorg_success = TRUE;
	int			data_fill_factor, index_fill_factor;
	int			reorg_op, reg_max_rec, reg_max_key, reg_max_blk;
	char			cli_buff[MAX_LINE], *ptr;
	glist			gl_head, exclude_gl_head, *gl_ptr;
	uint4			cli_status;
	unsigned short		n_len;

	error_def(ERR_NOSELECT);
	error_def(ERR_NOEXCLUDE);
	error_def(ERR_EXCLUDEREORG);
	error_def(ERR_REORGINC);
	error_def(ERR_REORGCTRLY);
	error_def(ERR_MUNOFINISH);

	mu_outofband_setup();

	if ((CLI_PRESENT == cli_present("UPGRADE")) || (CLI_PRESENT == cli_present("DOWNGRADE")))
	{
		/* note that "mu_reorg_process" is not set to TRUE in case of MUPIP REORG UPGRADE/DOWNGRADE.
		 * this is intentional because we are not doing any REORG kind of processing.
		 */
		mu_reorg_upgrd_dwngrd();
		mupip_exit(SS_NORMAL);	/* does not return */
	}

	resume = (CLI_PRESENT == cli_present("RESUME"));
	reorg_op = DEFAULT;
	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT == cli_present("USER_DEFINED_REORG") && (CLI_GET_STR_ALL("USER_DEFINED_REORG", cli_buff, &n_len)))
	{
		for (ptr = cli_buff; ; )
		{
			if (0 == STRNCMP_LIT(ptr, "SWAPHIST"))
				reorg_op |= SWAPHIST;
			else if (0 == STRNCMP_LIT(ptr, "NOCOALESCE"))
				reorg_op |= NOCOALESCE;
			else if (0 == STRNCMP_LIT(ptr, "NOSPLIT"))
				reorg_op |= NOSPLIT;
			else if (0 == STRNCMP_LIT(ptr, "NOSWAP"))
				reorg_op |= NOSWAP;
			else if (0 == STRNCMP_LIT(ptr, "DETAIL"))
				reorg_op |= DETAIL;
			ptr  = (char *)strchr(ptr, ',');
			if (ptr)
				ptr++;
			else
				break;
		}
	}
	if ((cli_status = cli_present("FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(data_fill_factor) == SIZEOF(int4));
		if (!cli_get_int("FILL_FACTOR", (int4 *)&data_fill_factor) || MAX_FILLFACTOR < data_fill_factor)
			data_fill_factor = MAX_FILLFACTOR;
		else if (MIN_FILLFACTOR > data_fill_factor)
			data_fill_factor = MIN_FILLFACTOR;
	}
	else
		data_fill_factor = MAX_FILLFACTOR;
	if ((cli_status = cli_present("INDEX_FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(index_fill_factor) == SIZEOF(int4));
		if (!cli_get_int("INDEX_FILL_FACTOR", (int4 *)&index_fill_factor))
			index_fill_factor = data_fill_factor;
		else if (MIN_FILLFACTOR > index_fill_factor)
			index_fill_factor = MIN_FILLFACTOR;
		else if (MAX_FILLFACTOR < index_fill_factor)
			index_fill_factor = MAX_FILLFACTOR;
	}
	else
		index_fill_factor = data_fill_factor;
	util_out_print("Fill Factor:: Index blocks !UL%: Data blocks !UL%", FLUSH, index_fill_factor, data_fill_factor);
	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT != cli_present("EXCLUDE"))
		exclude_gl_head.next = NULL;
	else if (FALSE == CLI_GET_STR_ALL("EXCLUDE", cli_buff, &n_len))
		exclude_gl_head.next = NULL;
	else
	{
		/* gv_select will select globals for this clause */
		gv_select(cli_buff, n_len, FALSE, "EXCLUDE", &exclude_gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk);
		if (!exclude_gl_head.next)
			gtm_putmsg(VARLSTCNT(1) ERR_NOEXCLUDE);
	}

	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT != cli_present("SELECT"))
	{
		n_len = 1;
                cli_buff[0] = '*';
	}
	else if (FALSE == CLI_GET_STR_ALL("SELECT", cli_buff, &n_len))
	{
		n_len = 1;
                cli_buff[0] = '*';
	}
	/* gv_select will select globals for this clause */
	gv_select(cli_buff, n_len, FALSE, "SELECT", &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk);
	if (!gl_head.next)
	{
		rts_error(VARLSTCNT(1) ERR_NOSELECT);
		mupip_exit(ERR_NOSELECT);
	}

	mu_reorg_process = TRUE;
	assert(NULL == gv_currkey_next_reorg);
	GVKEY_INIT(gv_currkey_next_reorg, DBKEYSIZE(MAX_KEY_SZ));
	reorg_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);
	for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		util_out_print("   ", FLUSH);
		util_out_print("Global: !AD ", FLUSH, gl_ptr->name.str.len, gl_ptr->name.str.addr);
		if (in_exclude_list((uchar_ptr_t)gl_ptr->name.str.addr, gl_ptr->name.str.len, &exclude_gl_head))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_EXCLUDEREORG, 2, gl_ptr->name.str.len, gl_ptr->name.str.addr);
			reorg_success = FALSE;
			continue;
		}
		reorg_success &= mu_reorg(&gl_ptr->name, &exclude_gl_head, &resume, index_fill_factor, data_fill_factor, reorg_op);
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
		{
			gtm_putmsg(VARLSTCNT(1) ERR_REORGCTRLY);
			mupip_exit(ERR_MUNOFINISH);
		}
	}
	if (!reorg_success)
	{
		rts_error(VARLSTCNT(1) ERR_REORGINC);
		mupip_exit(ERR_REORGINC);
	}
	else
		mupip_exit(SS_NORMAL);
}

