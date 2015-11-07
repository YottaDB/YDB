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

#define MAX_UTIL_LEN 80

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
			rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (cs_addrs->now_crit)
		{
			util_out_print("!/Write critical section already seized.!/", TRUE);
			return;
		}
		crash_count = cs_addrs->critical->crashcnt;
		grab_crit(gv_cur_region);
		cs_addrs->hold_onto_crit = TRUE;	/* need to do this AFTER grab_crit */
		cs_addrs->dse_crit_seize_done = TRUE;
		util_out_print("!/Seized write critical section.!/", TRUE);
		if (!cycle)
			return;
	}
	if (cli_present("RELEASE") == CLI_PRESENT || cycle)
	{
		if (gv_cur_region->read_only && !cycle)
			rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (!cs_addrs->now_crit)
		{
			util_out_print("!/Critical section already released.!/", TRUE);
			return;
		}
		crash_count = cs_addrs->critical->crashcnt;
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
			rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		cs_addrs->hdr->image_count = 0;
		UNIX_ONLY(gtm_mutex_init(gv_cur_region, NUM_CRIT_ENTRY(cs_addrs->hdr), crash));
		VMS_ONLY(mutex_init(cs_addrs->critical, NUM_CRIT_ENTRY(cs_addrs->hdr), crash));
		cs_addrs->nl->in_crit = 0;
		cs_addrs->now_crit = FALSE;
		util_out_print("!/Reinitialized critical section.!/", TRUE);
		return;
	}
	if (cli_present("REMOVE") == CLI_PRESENT)
	{
		if (gv_cur_region->read_only)
			rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		if (cs_addrs->nl->in_crit == 0)
		{
			util_out_print("!/The write critical section is unowned!/", TRUE);
			return;
		}
		UNIX_ONLY(assert(LOCK_AVAILABLE != cs_addrs->critical->semaphore.u.parts.latch_pid);)
		VMS_ONLY(assert(cs_addrs->critical->semaphore >= 0);)
		cs_addrs->now_crit = TRUE;
		cs_addrs->nl->in_crit = process_id;
		crash_count = cs_addrs->critical->crashcnt;
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
		memcpy(util_buff, "!/Critical section crash count is ", 34);
		util_len = 34;
		util_len += i2hex_nofill(cs_addrs->critical->crashcnt, (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], "!/", 2);
		util_len += 2;
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
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
				UNIX_ONLY(util_out_print("Database !AD : CRIT Owned by pid [!UL]", TRUE,
						DB_LEN_STR(gv_cur_region), cs_addrs->nl->in_crit);)
				VMS_ONLY(util_out_print("Database !AD : CRIT owned by pid [0x!XL]", TRUE,
						DB_LEN_STR(gv_cur_region), cs_addrs->nl->in_crit);)
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
#		if defined(UNIX)
		util_out_print("!/Write critical section owner is process id !UL", TRUE, cs_addrs->nl->in_crit);
		if (cs_addrs->now_crit)
			util_out_print("DSE (process id:  !UL) owns the write critical section", TRUE, process_id);
#		elif defined(VMS)
		util_out_print("!/Write critical section owner is process id !XL", TRUE, cs_addrs->nl->in_crit);
		if (cs_addrs->now_crit)
			util_out_print("DSE (process id:  !XL) owns the write critical section", TRUE, process_id);
#		endif
		util_out_print(0, TRUE);
	} else
		util_out_print("!/Write critical section is currently unowned", TRUE);
	return;
}
