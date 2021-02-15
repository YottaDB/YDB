/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "cli.h"
#include "lockconst.h"
#include "wcs_recover.h"
#include "dse.h"
#include "tp_change_reg.h"	/* for tp_change_reg() prototype */

#ifdef UNIX
#include "mutex.h"
#endif
#include "util.h"

GBLREF sgmnt_addrs		*cs_addrs;
GBLREF gd_region		*gv_cur_region;
GBLREF uint4			process_id;
GBLREF short			crash_count;
GBLREF gd_addr			*original_header;

error_def(ERR_DBRDONLY);

void dse_crit(void)
{
	int			util_len, dse_crit_count;
	char			util_buff[MAX_UTIL_LEN];
	boolean_t		crash = FALSE, cycle = FALSE, owner = FALSE;
	gd_region		*save_region, *r_local, *r_top;

	crash = ((cli_present("CRASH") == CLI_PRESENT) || (cli_present("RESET") == CLI_PRESENT));
	cycle = (CLI_PRESENT == cli_present("CYCLE"));
	if (cli_present("SEIZE") == CLI_PRESENT || cycle)
	{
		if (gv_cur_region->read_only && !cycle)
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (cs_addrs->now_crit)
		{
			util_out_print("!/Write critical section already seized.!/", TRUE);
			return;
		}
		UPDATE_CRASH_COUNT(cs_addrs, crash_count);
		grab_crit_encr_cycle_sync(gv_cur_region, WS_58);
		cs_addrs->hold_onto_crit = TRUE;	/* need to do this AFTER grab_crit */
		cs_addrs->dse_crit_seize_done = TRUE;
		util_out_print("!/Seized write critical section.!/", TRUE);
		if (!cycle)
			return;
	}
	if (cli_present("RELEASE") == CLI_PRESENT || cycle)
	{
		if (gv_cur_region->read_only && !cycle)
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (!cs_addrs->now_crit)
		{
			util_out_print("!/Critical section already released.!/", TRUE);
			return;
		}
		UPDATE_CRASH_COUNT(cs_addrs, crash_count);
		if (cs_addrs->now_crit)
		{	/* user wants crit to be released unconditionally so "was_crit" not checked like everywhere else */
			assert(cs_addrs->hold_onto_crit && cs_addrs->dse_crit_seize_done);
			cs_addrs->dse_crit_seize_done = FALSE;
			cs_addrs->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
			rel_crit(gv_cur_region);
			util_out_print("!/Released write critical section.!/", TRUE);
		}
#		ifdef DEBUG
		else
			assert(!cs_addrs->hold_onto_crit && !cs_addrs->dse_crit_seize_done);
#		endif
		return;
	}
	if (cli_present("INIT") == CLI_PRESENT)
	{
		if (gv_cur_region->read_only)
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		cs_addrs->hdr->image_count = 0;
#		ifdef CRIT_USE_PTHREAD_MUTEX
		crash = TRUE;
#		endif
		gtm_mutex_init(gv_cur_region, NUM_CRIT_ENTRY(cs_addrs->hdr), crash);
		cs_addrs->nl->in_crit = 0;
		cs_addrs->now_crit = FALSE;
		util_out_print("!/Reinitialized critical section.!/", TRUE);
		return;
	}
	if (cli_present("REMOVE") == CLI_PRESENT)
	{
		if (gv_cur_region->read_only)
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (cs_addrs->nl->in_crit == 0)
		{
			util_out_print("!/The write critical section is unowned!/", TRUE);
			return;
		}
#		ifndef CRIT_USE_PTHREAD_MUTEX
		assert(LOCK_AVAILABLE != cs_addrs->critical->semaphore.u.parts.latch_pid);
#		endif
		cs_addrs->now_crit = TRUE;
		cs_addrs->nl->in_crit = process_id;
		UPDATE_CRASH_COUNT(cs_addrs, crash_count);
		/* user wants crit to be removed unconditionally so "was_crit" not checked (before rel_crit) like everywhere else */
		if (dba_bg == cs_addrs->hdr->acc_meth)
		{
			wcs_recover(gv_cur_region);
			/* In case, this crit was obtained through a CRIT -SEIZE, csa->hold_onto_crit would have been set to
			 * TRUE. Set that back to FALSE now that we are going to release control of crit.
			 */
			cs_addrs->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
			cs_addrs->dse_crit_seize_done = FALSE;
			rel_crit(gv_cur_region);
			util_out_print("!/Removed owner of write critical section!/", TRUE);
		} else
		{
			/* In case, this crit was obtained through a CRIT -SEIZE, csa->hold_onto_crit would have been set to
			 * TRUE. Set that back to FALSE now that we are going to release control of crit.
			 */
			cs_addrs->hold_onto_crit = FALSE;	/* need to do this before the rel_crit */
			cs_addrs->dse_crit_seize_done = FALSE;
			rel_crit(gv_cur_region);
			util_out_print("!/Removed owner of write critical section!/", TRUE);
			util_out_print("!/WARNING: No recovery because database is MM.!/", TRUE);
		}
		return;
	}
	if (crash)
	{
#		ifndef CRIT_USE_PTHREAD_MUTEX
		memcpy(util_buff, "!/Critical section crash count is ", 34);
		util_len = 34;
		util_len += i2hex_nofill(cs_addrs->critical->crashcnt, (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], "!/", 2);
		util_len += 2;
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
#		endif
		return;
	}
	if (cli_present("ALL") == CLI_PRESENT)
	{
		dse_crit_count = 0;
		save_region = gv_cur_region;
		for (r_local = original_header->regions, r_top = r_local + original_header->n_regions; r_local < r_top; r_local++)
		{
			if (!r_local->open || r_local->was_open)
				continue;
			gv_cur_region = r_local;
			tp_change_reg();
			if (cs_addrs->nl->in_crit)
			{
				dse_crit_count++;
				util_out_print("Database !AD : CRIT Owned by pid [!UL]", TRUE,
						DB_LEN_STR(gv_cur_region), cs_addrs->nl->in_crit);
			}
		}
		if (0 == dse_crit_count)
			util_out_print("CRIT is currently unowned on all regions", TRUE);
		gv_cur_region = save_region;
		tp_change_reg();
		return;
	}
	if (cs_addrs->nl->in_crit)
	{
		util_out_print("!/Write critical section owner is process id !UL", TRUE, cs_addrs->nl->in_crit);
		if (cs_addrs->now_crit)
			util_out_print("DSE (process id:  !UL) owns the write critical section", TRUE, process_id);
		util_out_print(0, TRUE);
	} else
		util_out_print("!/Write critical section is currently unowned", TRUE);
	return;
}
