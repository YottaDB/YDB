/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include "locks.h"
#include <lckdef.h>
#include <psldef.h>
#include <efndef.h>


error_def(ERR_GTMCHECK);


/* AST routine entered on completion of sys$qio to write transaction history to disk, in ccp_exitwm2a;
   now we may drop write mode and start writing dirty buffers */

void	ccp_exitwm3( ccp_db_header *db)
{
	sgmnt_addrs		*csa;
	bt_rec			*que_base, *que_top, *p;
	ccp_action_record	request;
	uint4		status;


	assert(lib$ast_in_prog());

	csa = db->segment;
	assert(csa->nl->ccp_state == CCST_WMXREQ);
	assert(csa->ti->curr_tn == csa->ti->early_tn);

	db->wm_iosb.valblk[CCP_VALBLK_TRANS_HIST] = csa->ti->curr_tn + csa->ti->lock_sequence;
	if (JNL_ENABLED(csa->hdr)  &&  csa->jnl != NULL)
	{
		assert(csa->jnl->channel != 0);
		db->wm_iosb.valblk[CCP_VALBLK_JNL_ADDR] = csa->jnl->jnl_buff->freeaddr;
		db->wm_iosb.valblk[CCP_VALBLK_EPOCH_TN] = csa->jnl->jnl_buff->epoch_tn;
		/* lastaddr is no longer a field in jnl_buff
		 *	db->wm_iosb.valblk[CCP_VALBLK_LST_ADDR] = csa->jnl->jnl_buff->lastaddr;
		 */
	}

	/* Convert Write-mode lock from Protected Write to Concurrent Read, writing the lock value block */
	status = ccp_enqw(EFN$C_ENF, LCK$K_CRMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
			  NULL, 0, NULL, PSL$C_USER, 0);
	/***** Check error status here? *****/

	for (que_base = csa->bt_header, que_top = que_base + csa->hdr->bt_buckets;
	     que_base < que_top;
	     ++que_base)
	{
		assert(que_base->blk == BT_QUEHEAD);

		for (p = (bt_rec *)((char *)que_base + que_base->blkque.fl);
		     p != que_base;
		     p = (bt_rec *)((char *)p + p->blkque.fl))
		{
			if (((int4)p & 3) != 0)
				ccp_signal_cont(ERR_GTMCHECK);	/***** Is this reasonable? *****/
			p->flushing = FALSE;
		}
	}

	db->blocking_ast_received = FALSE;
	db->wmexit_requested = FALSE;
	csa->nl->ccp_state = CCST_WMXGNT;
	db->wc_rover = 0;

	request.action = CCTR_EWMWTBF;
	request.pid = 0;
	request.v.h = db;
	ccp_act_request(&request);

	return;
}
