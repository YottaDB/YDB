/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "mlkdef.h"
#include "jnl.h"
#include "locks.h"
#include <lckdef.h>
#include <psldef.h>
#include <ssdef.h>
#include "crit_wake.h"


#define NODENUMBER	0xFFE00000

GBLREF	uint4	process_id;

/* TODO: move to a header */
void			ccp_quantum_interrupt(), ccp_reqdrtbuf_interrupt();

static	int4		delta_100_msec[2] = { -1000000, -1 };


/* Request for write mode;  entered after successful conversion of Write-mode lock to Protected Write */

void	ccp_tr_writedb1(ccp_action_record *rec)
{
	ccp_action_record	request;
	ccp_db_header		*db;
	jnl_buffer		*jb;
	sgmnt_addrs		*csa;
	int4			curr_time[2];
	uint4		status, *p1, *p2, *top;


	if ((db = rec->v.h) == NULL)
		return;

	csa = db->segment;
	assert(csa->nl->ccp_state == CCST_RDMODE  ||  csa->nl->ccp_state == CCST_CLOSED  ||  csa->nl->ccp_state == CCST_OPNREQ);

	if (JNL_ENABLED(csa->hdr))
	{
		assert(csa->jnl->channel != 0);

		jb = csa->jnl->jnl_buff;

		if (jb->blocked != 0)
		{
			/* jnl writes from a previous write mode are not done yet;  try again later */
			if ((status = sys$setimr(0, delta_100_msec, ccp_writedb5, db, 0)) != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			return;
		}

		jb->epoch_tn = db->wm_iosb.valblk[CCP_VALBLK_EPOCH_TN];
		jb->freeaddr = jb->dskaddr
			     = ROUND_UP(db->wm_iosb.valblk[CCP_VALBLK_JNL_ADDR], DISK_BLOCK_SIZE);
		/* lastaddr is no longer a field in jnl_buff
		 *	jb->lastaddr = db->wm_iosb.valblk[CCP_VALBLK_LST_ADDR];
		 */
		jb->free = jb->dsk
			 = 0;
	}

	/* Note:  We must clear these flags prior to changing state or a set from ccp_exitwm_blkast may be missed */
	db->quantum_expired = FALSE;
	db->tick_in_progress = FALSE;

	if (db->remote_wakeup)
	{
		for (p2 = NULL, p1 = csa->lock_addrs[0], top = p1 + SIZEOF(int4) * NUM_CLST_LCKS;
		     *p1 != 0  &&  p1 <= top;
		     ++p1)
		{
			if ((*p1 & NODENUMBER) == (process_id & NODENUMBER))
			{
				crit_wake(p1);
				if (p2 == NULL)
					p2 = p1;
				*p1 = 0;
			}
			else
				if (p2 != NULL)
				{
					*p2++ = *p1;
					*p1 = 0;
				}
		}

		db->remote_wakeup = FALSE;

		status = ccp_enq(0, LCK$K_CRMODE, &db->lock_iosb, LCK$M_CONVERT | LCK$M_SYNCSTS, NULL, 0,
				 ccp_lkrqwake1, db, ccp_lkdowake_blkast, PSL$C_USER, 0);
		/***** Check error status here? *****/
	}

	db->glob_sec->freeze = 0;
	/* db->glob_sec->wcs_active_lvl = db->glob_sec->n_bts / 2; */
	db->drop_lvl = 0;

	sys$gettim(curr_time);
	lib$add_times(curr_time, db->glob_sec->ccp_quantum_interval, &db->start_wm_time);

	csa->nl->ccp_state = CCST_WRTGNT;

	if (db->blocking_ast_received)
	{
		status = sys$dclast(ccp_exitwm_blkast, &db->exitwm_timer_id, PSL$C_USER);
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	csa->nl->ccp_crit_blocked = FALSE;
	++csa->nl->ccp_cycle;

	status = sys$setimr(0, &db->glob_sec->ccp_quantum_interval, ccp_quantum_interrupt, &db->quantum_timer_id, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	db->dirty_buffers_acquired = FALSE;
	ccp_pndg_proc_wake(&db->write_wait);

	status = ccp_enq(0, LCK$K_EXMODE, &db->flush_iosb, LCK$M_CONVERT | LCK$M_SYNCSTS, NULL, 0,
			 ccp_reqdrtbuf_interrupt, db, NULL, PSL$C_USER, 0);
	if (status == SS$_SYNCH)
	{
		request.action = CCTR_GOTDRT;
		request.pid = 0;
		request.v.h = db;
		ccp_act_request(&request);
	}
	/***** Check error status here? *****/

}
