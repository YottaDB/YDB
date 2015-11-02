/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsblk.h"
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "error.h"
#include "jnl.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "tp.h"
#include "tp_restart.h"
#include "util.h"

GBLREF	sgm_info		*first_sgm_info;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	int			tprestart_state;

error_def(ERR_TPRETRY);

#ifdef GTM_TRIGGER
/* This code is modeled around "updproc_ch" in updproc.c and is used by the trigger_* modules */
CONDITION_HANDLER(trigger_tpwrap_ch)
{
	int	rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		tprestart_state = TPRESTART_STATE_NORMAL;
		assert(NULL != first_sgm_info);
		/* This only happens at the outer-most layer so state should be normal now */
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		assert(0 == rc);
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		/* "reset_gv_target" might have been set to a non-default value if we are deep inside "gvcst_put"
		 * when the restart occurs. Reset it before unwinding the gvcst_put C stack frame. Normally gv_target would
		 * be set to what is in reset_gv_target (using the RESET_GV_TARGET macro) but that could lead to gv_target
		 * and gv_currkey going out of sync depending on where in gvcst_put we got the restart (e.g. if we got it
		 * in gvcst_root_search before gv_currkey was initialized but after gv_target was). Therefore we instead set
		 * "reset_gv_target" back to its default value leaving "gv_target" untouched. This is ok to do so since as
		 * part of the tp restart, gv_target and gv_currkey are anyways going to be reset to what they were at the
		 * beginning of the TSTART and therefore are guaranteed to be back in sync. Not resetting "reset_gv_target"
		 * would also cause an assert (on this being the invalid) in "gvtr_match_n_invoke" to fail in a restart case.
		 */
		DBGEHND((stderr, "trigger_tpwrap_ch: TP restart encountered\n"));
		reset_gv_target = INVALID_GV_TARGET;
		UNWIND(NULL, NULL);
	}
	DBGEHND((stderr, "trigger_tp_wrap_ch: Condition handler passing signal %d on to next handler\n", SIGNAL));
	NEXTCH;
}
#endif
