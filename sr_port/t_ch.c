/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "have_crit.h"

GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF	boolean_t		dont_want_core;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	short			dollar_tlevel;

CONDITION_HANDLER(t_ch)
{
	boolean_t	retvalue;

	error_def(ERR_GTMCHECK);
	error_def(ERR_GTMASSERT);
	error_def(ERR_ASSERT);
        error_def(ERR_MEMORY);
        error_def(ERR_VMSMEMORY);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_OUTOFSPACE);

	START_CH;
	UNIX_ONLY(
		/* To get as virgin a state as possible in the core, take the core now if we
		 * would be doing so anyway. This will set created_core so it doesn't happen again.
		 */
		if (DUMPABLE)
		{	/* this is most likely a fatal error, therefore print the error right here as we do not know if
			 * the send_msg() call in t_commit_cleanup() done below might overlay this primary fatal error.
			 */
			PRN_ERROR;
			if (!SUPPRESS_DUMP)
			{
				need_core = TRUE;
				gtm_fork_n_core();
			}
		}
	)
	ENABLE_AST;
	/* We could go through all regions involved in the TP and check for crit on only those regions - instead we use an existing
	 * function: have_crit().  If the design assumption that all crits held at transaction commit time are
	 * transaction related holds true, the result is the same and efficiency doesn't matter (too much) in exception handling.
	 */
	if ((!dollar_tlevel && cs_addrs->now_crit) || (dollar_tlevel && 0 != have_crit(CRIT_HAVE_ANY_REG)))
	{
		retvalue = t_commit_cleanup(cdb_sc_uperr, SIGNAL); /* if return value is TRUE, it means transaction commit has */
		assert(!retvalue); 				   /* started in which case we should not have come to t_ch()  */
								   /* instead t_end/tp_tend would have called t_commit_cleanup */
	}
	VMS_ONLY(
		if (SEVERITY == INFO)	/* this turns the info messages from cert_blk into fatals because other approaches */
		{			/* to that problem seem to get tangled in the vms condition handler system */
			TERMINATE;
		}
	)
	NEXTCH;
}
