/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_stdio.h"
#include "util.h"
#include "cli.h"
#include "gdsroot.h"
#include "gt_timer.h"
#include "gtmmsg.h"

#if defined(UNIX)
#include "gtm_ipc.h"
#endif

#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdskill.h"
#include "gdscc.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "dse.h"
#ifdef UNIX
# include "mutex.h"
#endif
#include "wcs_flu.h"
#include "gtm_signal.h"

GBLREF	VSIG_ATOMIC_T	util_interrupt;
GBLREF	block_id	patch_curr_blk;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	short		crash_count;
GBLREF	uint4		process_id;
GBLREF	gd_addr		*original_header;
GBLREF	boolean_t	dse_all_dump;		/* TRUE if DSE ALL -DUMP is specified */

error_def(ERR_DBRDONLY);
error_def(ERR_DSEWCINITCON);
error_def(ERR_FREEZE);
error_def(ERR_FREEZECTRL);

void dse_all(void)
{
	gd_region	*reg;
	tp_region	*region_list, *rg, *rg_last, *rg_new;	/* A handy structure for maintaining a list of regions */
	int		i;
	sgmnt_addrs	*old_addrs, *csa;
	sgmnt_data_ptr_t	csd;
	gd_region	*old_region;
	block_id	old_block;
	int4		stat;
	boolean_t	ref = FALSE;
	boolean_t	crit = FALSE;
	boolean_t	wc = FALSE;
	boolean_t	flush = FALSE;
	boolean_t	freeze = FALSE;
	boolean_t	nofreeze = FALSE;
	boolean_t	seize = FALSE;
	boolean_t	release = FALSE;
	boolean_t	dump = FALSE;
	boolean_t	override = FALSE;
	boolean_t	was_crit;
	boolean_t	clear_corrupt = FALSE;
	UNIX_ONLY(char	*fgets_res;)

	old_addrs = cs_addrs;
	old_region = gv_cur_region;
	old_block = patch_curr_blk;
	if (cli_present("RENEW") == CLI_PRESENT)
	{
		crit = ref = wc = nofreeze = TRUE;
		GET_CONFIRM_AND_HANDLE_NEG_RESPONSE;
	} else
	{
		if (cli_present("CRITINIT") == CLI_PRESENT)
			crit = TRUE;
		if (cli_present("REFERENCE") == CLI_PRESENT)
			ref = TRUE;
		if (cli_present("WCINIT") == CLI_PRESENT)
		{
			GET_CONFIRM_AND_HANDLE_NEG_RESPONSE;
			wc = TRUE;
		}
		if (cli_present("BUFFER_FLUSH") == CLI_PRESENT)
			flush = TRUE;
		if (cli_present("SEIZE") == CLI_PRESENT)
			seize = TRUE;
		if (cli_present("RELEASE") == CLI_PRESENT)
			release = TRUE;
		stat = cli_present("FREEZE");
		if (stat == CLI_NEGATED)
			nofreeze = TRUE;
		else if (stat == CLI_PRESENT)
		{
			freeze = TRUE;
			nofreeze = FALSE;
		}
                if (cli_present("OVERRIDE") == CLI_PRESENT)
                        override = TRUE;
                if (cli_present("DUMP") == CLI_PRESENT)
			dump = TRUE;
                if (cli_present("CLEARCORRUPT") == CLI_PRESENT)
			clear_corrupt = TRUE;
	}
        if (!dump && gv_cur_region->read_only)
                rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	region_list = NULL;
	for (i = 0, reg = original_header->regions; i < original_header->n_regions; i++, reg++)
	{
		if (!IS_REG_BG_OR_MM(reg))
		{
			util_out_print("Skipping region !AD: not BG or MM access", TRUE, REG_LEN_STR(reg));
			continue;
		}
		if (!reg->open)
		{
			if (!IS_STATSDB_REG(reg))
				util_out_print("Skipping region !AD as it is not bound to any namespace.", TRUE, REG_LEN_STR(reg));
			continue;
		}
		if (dump)
		{
			gv_cur_region = reg;
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
			dse_all_dump = TRUE;
			dse_dmp_fhead();
			assert(!dse_all_dump);	/* should have been reset by "dse_dmp_fhead" */
		} else
			/* put on region list in order of ftok value so processed in same order that crits are obtained */
			insert_region(reg, &(region_list), NULL, SIZEOF(tp_region));
	}
	if (!dump)
	{	/* Now run the list of regions in the sorted ftok order to execute the desired commands */
		for (rg = region_list; NULL != rg; rg = rg->fPtr)
		{
			gv_cur_region = rg->reg;
			assert(IS_REG_BG_OR_MM(gv_cur_region));
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
			csd = cs_addrs->hdr;
			patch_curr_blk = get_dir_root();
			if (crit)
			{
				UNIX_ONLY(gtm_mutex_init(gv_cur_region, NUM_CRIT_ENTRY(csd), TRUE));
				VMS_ONLY(mutex_init(cs_addrs->critical, NUM_CRIT_ENTRY(csd), TRUE));
				cs_addrs->nl->in_crit = 0;
				cs_addrs->hold_onto_crit = FALSE;	/* reset this just before cs_addrs->now_crit is reset */
				cs_addrs->now_crit = FALSE;
			}
			if (cs_addrs->critical)
				crash_count = cs_addrs->critical->crashcnt;
			if (freeze)
			{
				while (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, TRUE, override, FALSE, FALSE, FALSE))
				{
					hiber_start(1000);
					if (util_interrupt)
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_FREEZECTRL);
                        	                break;
					}
				}
				if (freeze != !(FROZEN(csd)))
				{
					util_out_print("Region !AD is now FROZEN", TRUE, REG_LEN_STR(gv_cur_region));
				}
			}
			was_crit = cs_addrs->now_crit;
			if (seize)
			{
				if (!was_crit)	/* No point seizing crit if WE already have it held */
					grab_crit_encr_cycle_sync(gv_cur_region);
				cs_addrs->hold_onto_crit = TRUE; /* Need to do this AFTER grab_crit */
				cs_addrs->dse_crit_seize_done = TRUE;
			}
			if (wc)
			{
				if (!was_crit && !seize)
					grab_crit_encr_cycle_sync(gv_cur_region);
				DSE_WCREINIT(cs_addrs);
				if (!was_crit && (!seize || release))
					rel_crit(gv_cur_region);
			}
			if (flush)
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
			if (release)
			{	/* user wants crit to be released unconditionally so "was_crit" not checked like everywhere else */
				if (cs_addrs->now_crit)
				{
					cs_addrs->dse_crit_seize_done = FALSE;
					cs_addrs->hold_onto_crit = FALSE; /* need to do this BEFORE rel_crit */
					rel_crit(gv_cur_region);
				}
				else
				{
					assert(!cs_addrs->hold_onto_crit && !cs_addrs->dse_crit_seize_done);
					util_out_print("Current process does not own the Region: !AD.",
						TRUE, REG_LEN_STR(gv_cur_region));
				}
			}
			if (nofreeze)
			{
				if (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, FALSE, override, FALSE, FALSE, FALSE))
					util_out_print("Region: !AD is frozen by another user, not releasing freeze", TRUE,
						REG_LEN_STR(gv_cur_region));
				else
					util_out_print("Region !AD is now UNFROZEN", TRUE, REG_LEN_STR(gv_cur_region));
			}
			if (ref)
				cs_addrs->nl->ref_cnt = 1;
			if (clear_corrupt)
				csd->file_corrupt = FALSE;
		}
	}
	cs_addrs = old_addrs;
	gv_cur_region = old_region;
	patch_curr_blk = old_block;
	return;
}
