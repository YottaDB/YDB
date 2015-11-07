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
#include <fab.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include <iodef.h>
#include <ssdef.h>
#include "ccp_writedb2.h"

GBLREF	uint4		process_id;
GBLREF sgmnt_addrs	*vms_mutex_check_csa;


static	int4		delta_100_msec[2] = { -100000, -1 };


/* AST routine entered on completion of PW mode lock conversion request in ccp_request_write_mode */

void ccp_reqwm_interrupt(ccp_db_header **pdb)
{
	ccp_db_header	*db;
	sgmnt_addrs	*csa;
	uint4	status;


	assert(lib$ast_in_prog());

	db = *pdb;

	csa = db->segment;
	if (csa == NULL  ||  csa->nl->ccp_state == CCST_CLOSED)
		return;
	vms_mutex_check_csa = csa;
	switch (db->wm_iosb.cond)
	{
	case SS$_DEADLOCK:
		ccp_signal_cont(SS$_DEADLOCK);
		/* Just try again */
		ccp_request_write_mode(db);
		return;

	case SS$_CANCEL:
		/* Lock cancelled by close */
		return;

	case SS$_VALNOTVALID:
		/* Force reads from disk */
		db->wm_iosb.valblk[CCP_VALBLK_TRANS_HIST] = 0;
		db->last_lk_sequence = db->master_map_start_tn
				     = 0;
		/* Drop through ... */

	case SS$_NORMAL:
		if (db->wm_iosb.valblk[CCP_VALBLK_TRANS_HIST] == csa->ti->curr_tn + csa->ti->lock_sequence)
		{
			/* No change to current tn, do not need to update header */
			if (csa->now_crit)
			{
				assert (csa->nl->in_crit == process_id);
				csa->nl->in_crit = 0;
				(void)mutex_unlockw(csa->critical, csa->critical->crashcnt, &csa->now_crit);
				/***** Check error status here? *****/
			}
			ccp_writedb5(db);
		}
		else
		{
			if (csa->nl->in_crit == 0)
			{
				if (mutex_lockwim(csa->critical, csa->critical->crashcnt, &csa->now_crit) == cdb_sc_normal)
					csa->nl->in_crit = process_id;		/* now_crit was set by mutex_lockwim */
				else
					if (csa->nl->in_crit == 0)		/***** Why is this re-tested? *****/
					{
						status = sys$setimr(0, delta_100_msec, ccp_reqwm_interrupt, &db->wmcrit_timer_id,
								    0);
						if (status != SS$_NORMAL)
							ccp_signal_cont(status);	/***** Is this reasonable? *****/
						return;
					}
			}
			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_writedb2, db,
					 &db->glob_sec->trans_hist, BT_SIZE(csa->hdr) + SIZEOF(th_index), TH_BLOCK, 0, 0, 0);
			if (status != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
		}
		return;

	default:
		ccp_signal_cont(db->wm_iosb.cond);		/***** Is this reasonable? *****/
		return;
	}
}
