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

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "error.h"
#include "ast.h"
#include "send_msg.h"
#include "t_commit_cleanup.h"
#include "util.h"

GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF	boolean_t		dont_want_core;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	short			dollar_tlevel;
GBLREF	uint4			t_err;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int		t_tries;

CONDITION_HANDLER(t_ch)
{
	unsigned char	local_hist[CDB_MAX_TRIES + 2];	/* existing history + 2 */
	unsigned int	hist_index;

	error_def(ERR_GTMCHECK);
	error_def(ERR_GTMASSERT);
	error_def(ERR_ASSERT);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_OUTOFSPACE);

	START_CH;
	UNIX_ONLY(
		/* To get as virgin a state as possible in the core, take the core now if we
		 * would be doing so anyway. This will set created_core so it doesn't happen again.
		 */
		if (DUMPABLE && !SUPPRESS_DUMP)
		{
			need_core = TRUE;
			gtm_fork_n_core();
		}
	)
	ENABLE_AST;
	/* We could go through all regions involved in the TP and check for crit on only those regions - instead we use an existing 
	 * function: have_crit_any_region().  If the design assumption that all crits held at transaction commit time are 
	 * transaction related holds true, the result is the same and efficiency doesn't matter (too much) in exception handling.
	 */
	if (((!dollar_tlevel && cs_addrs->now_crit) || (dollar_tlevel && have_crit_any_region(FALSE)))
		&& t_commit_cleanup(cdb_sc_uperr, SIGNAL) && SIGNAL)
	{
		for (hist_index = 0;  hist_index < t_tries;  hist_index++)
			local_hist[hist_index] = t_fail_hist[hist_index];
		local_hist[t_tries]     = (unsigned char)cdb_sc_uperr;
		local_hist[t_tries + 1] = (unsigned char)cdb_sc_committfail;
		/* don't do a gtm_putmsg of this since that will reset SIGNAL to gtm_putmsg's argument which is not what we want. */
		send_msg(VARLSTCNT(4) t_err, 2, t_tries + 2, local_hist);
		PRN_ERROR;
	}
	VMS_ONLY(
		if (SEVERITY == INFO)	/* this turns the info messages from cert_blk into fatals because other approaches */
		{			/* to that problem seem to get tangled in the vms condition handler system */
			TERMINATE;
		}
	)
	NEXTCH;
}
