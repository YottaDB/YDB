/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "filestruct.h"
#include "jnl.h"

GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF	boolean_t		dont_want_core;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			dollar_tlevel;
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(t_ch)
{
	boolean_t	retvalue;

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
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{	/* We dont know how it is possible to have a success or info type severity message show up while we are in
		 * the middle of a transaction commit. In any case, return right away instead of invoking t_commit_cleanup.
		 * The issue with invoking that is that it would complete the transaction and release crit but the NEXTCH
		 * call at the end of this condition handler would invoke the next level condition handler which could
		 * decide the message is innocuous and therefore decide to return control to where the error was signalled
		 * in the first place (in the midst of database commit) which can cause database damage since we no longer
		 * hold crit.
		 */
		assert(FALSE);
		CONTINUE;
	}
	ENABLE_AST;
	/* Reset jgbl.dont_reset_gbl_jrec_time to FALSE if already set by tp_tend and we come here due to an rts_error in wcs_flu.
	 * However, if it was forward recovery that ended up invoking tp_tend, then we should not reset the variable to FALSE as
	 * forward recovery keeps it set to TRUE for its entire duration. So, take that into account for the if check.
	 */
	if (jgbl.dont_reset_gbl_jrec_time && dollar_tlevel && !jgbl.forw_phase_recovery)
		jgbl.dont_reset_gbl_jrec_time = FALSE;
	/* We could go through all regions involved in the TP and check for crit on only those regions - instead we use an
	 * existing function "have_crit".  If the design assumption that all crits held at transaction commit time are
	 * transaction related holds true, the result is the same and efficiency doesn't matter (too much) in exception handling.
	 */
	if ((!dollar_tlevel && T_IN_CRIT_OR_COMMIT(cs_addrs))
		|| (dollar_tlevel && (0 != have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT))))
	{
		retvalue = t_commit_cleanup(cdb_sc_uperr, SIGNAL); /* if return value is TRUE, it means transaction commit has */
		assert(!retvalue || DUMPABLE); 			   /* started in which case we should not have come to t_ch()  */
								   /* instead t_end/tp_tend would have called t_commit_cleanup */
	}
	NEXTCH;
}
