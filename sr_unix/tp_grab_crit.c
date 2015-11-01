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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "send_msg.h"
#include "mutex.h"
#include "tp_grab_crit.h"
#include "deferred_signal_handler.h"
#include "wcs_recover.h"
#include "have_crit_any_region.h"
#include "caller_id.h"

GBLREF	volatile boolean_t	crit_in_flux;
GBLREF	uint4 		process_id;
GBLREF	short 		crash_count;
GBLREF	boolean_t	forced_exit;
GBLREF	gd_region 	*gv_cur_region;
GBLREF	boolean_t	mutex_salvaged;

DEBUG_ONLY(
GBLREF	sgmnt_addrs		*cs_addrs;	/* for TP_CHANGE_REG macro */
GBLREF	sgmnt_data_ptr_t	cs_data;
)

bool	tp_grab_crit(gd_region *reg)
{
	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	int4			coidx;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;
	DEBUG_ONLY(gd_region	*r_save;)

	error_def(ERR_CRITRESET);
	error_def(ERR_DBCCERR);
	error_def(ERR_WCBLOCKED);

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	if (!csa->now_crit)
	{
		assert(FALSE == crit_in_flux);
		crit_in_flux = TRUE;	/* prevent interrupts */
		DEBUG_ONLY(r_save = gv_cur_region; TP_CHANGE_REG(reg)); /* for LOCK_HIST macro which is used only in DEBUG */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)&csa->hdr->mutex_spin_parms;
		status = mutex_lockwim(reg, mutex_spin_parms, crash_count);
		DEBUG_ONLY(TP_CHANGE_REG(r_save));	/* restore gv_cur_region */
		if (status != cdb_sc_normal)
		{
			crit_in_flux = FALSE;
			switch (status)
			{
				case cdb_sc_nolock:
					return(FALSE);
				case cdb_sc_critreset:
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					if (forced_exit && !have_crit_any_region(FALSE))
						deferred_signal_handler();
					GTMASSERT;
			}
			return(FALSE);
		}
		assert(csa->nl->in_crit == 0);
		csa->nl->in_crit = process_id;
		CRIT_TRACE(crit_ops_gw);		/* see gdsbt.h for comment on placement */
		if (mutex_salvaged) /* Mutex crash repaired, want to do write cache recovery, just in case */
		{
			SET_TRACEABLE_VAR(csa->hdr->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_tp_grab_crit);
			send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_tp_grab_crit"),
				process_id, csa->ti->curr_tn, REG_LEN_STR(reg));
		}
		crit_in_flux = FALSE;
	}
	if (csa->hdr->wc_blocked)
		wcs_recover(reg);
	return(TRUE);
}

