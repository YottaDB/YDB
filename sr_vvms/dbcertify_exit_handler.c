/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include <psldef.h>
#include <descrip.h>
#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gtm_inet.h"
#include "gtm_stdio.h"	/* for FILE structure etc. */
#include "gtm_time.h"

#include "ast.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "error.h"
#include "iotimer.h"
#include "jnl.h"
#include "locklits.h"
#include "gtmrecv.h"	/* for recvpool etc. */
#include "io.h"
#include "iosp.h"
#include "sleep_cnt.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sem.h"
#include "repl_shm.h"
#include "desblk.h"
#include "gtmimagename.h"
#include "util.h"
#include "op.h"
#include "repl_log.h"
#include "generic_exit_handler.h"
#include "gv_rundown.h"
#include "have_crit.h"
#include "print_exit_stats.h"
#include "setzdir.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtmmsg.h"
#include "secshr_db_clnup.h"
#include "gdsblk.h"
#include "gdsblkops.h"
#include "dbcertify.h"

GBLREF	int4			exi_condition;
GBLREF	desblk			exi_blk;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;
GBLREF	phase_static_area	*psa_gbl;

error_def(ERR_ACK);
error_def(ERR_FORCEDHALT);
error_def(ERR_UNKNOWNFOREX);

void dbcertify_exit_handler(void)
{
	void		(*signal_routine)();
	int4		lcnt;

	sys$setast(ENABLE);	/* safer and doesn't hurt much */
	sys$cantim(0,0);	/* cancel all outstanding timers.  prevents unwelcome surprises */

	/* We can defer exit-handling if it was a forced-halt and we are in an AST or have crit in any region.
	 * If we are in an AST when a fatal exception occurs we can neither defer exiting nor do normal exit-handling,
	 * 	so we return immediately with the hope that the privileged exit-handler in GTMSECSHR,
	 *	secshr_db_clnup(ABNORMAL_TERMINATION) will do the necessary cleanup.
	 * Note that even if we hold crit in any region when a non-deferrable exception occurs, we can still go ahead with
	 *	normal exit-handling chores. secshr_db_clnup(NORMAL_TERMINATION) (invoked below) will cleanup the crits for us.
	 */
	if (ERR_FORCEDHALT == exi_condition || 0 == exi_condition)
	{
		if (lib$ast_in_prog())	/* The rest of this assumes that it may use AST's */
		{
			EXIT_HANDLER(&exi_blk);				/* reestablish the exit handler */
			sys$forcex(&process_id);			/* make the forcex come back as an AST */
			ESTABLISH(exi_ch);				/* set a condition handler to unwind exit handler levels */
			rts_error_csa(CSA_ARG(NULL) ERR_ACK);		/* and signal success */
		}
		assert(!lib$ast_in_prog());
		/* We defer exiting only if we are in commit phase in any region as opposed to holding crit in that region.
		 * The danger of deferring if we are holding crit in a region is that we may do infinite defers in processes
		 *	that have no intention of releasing crit.
		 * But the commit phase (beginning from when early_tn is curr_tn + 1 to when they become equal) is a relatively
		 * 	finite window wherefrom we are guaranteed to return.
		 */
		if (psa_gbl->dbc_critical)
		{
			EXIT_HANDLER(&exi_blk);
			ESTABLISH(exi_ch);
			rts_error_csa(CSA_ARG(NULL) exi_condition ? exi_condition : ERR_FORCEDHALT);
		}
	} else if (lib$ast_in_prog())
		rts_error_csa(CSA_ARG(NULL) exi_condition);	/* this shouldn't return */
	SET_FORCED_EXIT_STATE_ALREADY_EXITING;
	print_exit_stats();
	if (0 == exi_condition)
		exi_condition = ERR_UNKNOWNFOREX;
	if (psa_gbl->phase_one)
		dbc_scan_phase_cleanup();
	else
		dbc_certify_phase_cleanup();
}
