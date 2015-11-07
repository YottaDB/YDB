/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <fab.h>
#include <iodef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include <ssdef.h>
#include "wcs_recover.h"
#include "crit_wake.h"


/****************************************************************************************
*	The scheme for managing exit write mode is explained in CCP_EXITWM_ATTEMPT.C.	*
*	Please do not update this code without updating the comments in that module.	*
****************************************************************************************/

error_def(ERR_CCPSIGDMP);
error_def(ERR_GTMASSERT);

/* Request to relinquish write mode */

void ccp_tr_exitwm( ccp_action_record *rec)
{
	ccp_db_header	*db;
	sgmnt_addrs	*cs_addrs;
	bt_rec		*que_base, *que_top, *p;
	int4		n, i;
	uint4	status;

	db = ccp_get_reg(&rec->v.exreq.fid);
	if (db == NULL)
		return;

	cs_addrs = db->segment;

	if (rec->v.exreq.cycle != db->segment->nl->ccp_cycle  ||  db->segment->nl->ccp_state != CCST_DRTGNT)
	{
		if (rec->pid != 0)
			if (db->segment->nl->ccp_state == CCST_WMXREQ)
				ccp_pndg_proc_add(&db->exitwm_wait, rec->pid);
			else
				crit_wake(&rec->pid);
		return;
	}

	if (rec->pid != 0)
		ccp_pndg_proc_add(&db->exitwm_wait, rec->pid);

	if (cs_addrs->nl->in_crit != 0  &&  cs_addrs->nl->in_crit != rec->pid)
		return;

	if (cs_addrs->nl->wc_blocked)
		wcs_recover(db->greg);

	assert(!db->tick_in_progress);
	assert(db->quantum_expired);
	assert(db->segment->nl->ccp_crit_blocked);
	assert(db->segment->nl->ccp_state == CCST_DRTGNT);
	assert(db->segment->ti->curr_tn == db->segment->ti->early_tn);

	db->segment->nl->ccp_state = CCST_WMXREQ;

	if (db->stale_timer_id != NULL)
	{
		status = sys$setimr(0, &db->glob_sec->staleness, ccp_staleness, &db->stale_timer_id, 0);
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
		db->stale_in_progress = TRUE;
	}

	db->last_write_tn = db->segment->ti->early_tn;

	for (;;)
	{
		for (que_base = db->segment->bt_header, que_top = que_base + db->glob_sec->n_bts;
		     que_base < que_top;
		     que_base++)
		{
			assert(que_base->blk == BT_QUEHEAD);
			i = 0;
			for (p = (bt_rec *)((char *)que_base + que_base->blkque.fl);
			     p != que_base;
			     p = (bt_rec *)((char *)p + p->blkque.fl))
			{
				if (((int4)p & 3) != 0)
					ccp_signal_cont(ERR_GTMASSERT);	/***** Is this reasonable? *****/
				n = p->cache_index;
				p->flushing = (n == -1) ? FALSE : ((cache_rec *)((char *)db->glob_sec + n))->dirty;
				if (i > db->glob_sec->n_bts)
					break;
				i++;
			}
			if (i > db->glob_sec->n_bts)
				break;
		}
		if (i > db->glob_sec->n_bts)
		{
			lib$signal(ERR_CCPSIGDMP, 1);
			wcs_recover(db->greg);
		}
		else
			break;
	}

	/* Update the master map if it has changed.  If the map is updated, then the master map ast completion
	   routine will start the transaction history update.  Otherwise, we will start it here.	*/
	if (db->master_map_start_tn < cs_addrs->ti->mm_tn)
	{
		status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_exitwm1, db,
				 MM_ADDR(db->glob_sec), MASTER_MAP_SIZE(db->glob_sec), MM_BLOCK, 0, 0, 0);
		if ((status & 1) == 0)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
		db->master_map_start_tn = cs_addrs->ti->mm_tn;
	}
	else
		if (db->last_lk_sequence < cs_addrs->ti->lock_sequence)
		{
			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_exitwm1a, db,
					 cs_addrs->lock_addrs[0], db->glob_sec->lock_space_size, LOCK_BLOCK(db->glob_sec) + 1,
					 0, 0, 0);
			if ((status & 1) == 0)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			db->last_lk_sequence = cs_addrs->ti->lock_sequence;
		}
		else
			ccp_exitwm2(db);

	return;
}
