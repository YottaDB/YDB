/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
#include "gdsbgtr.h"
#include "ccp.h"
#include <psldef.h>
#include "jnl.h"
#include "locks.h"


void	ccp_tr_closejnl( ccp_action_record *rec)
{
	ccp_db_header		*db;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	uint4		status;


	db = ccp_get_reg(&FILE_INFO(rec->v.reg)->file_id);

	assert(db->segment->nl->ccp_state == CCST_WMXGNT);

	csa = &FILE_INFO(db->greg)->s_addrs;

	if (csa->hdr->jnl_state != jnl_closed)
		return;

	jpc = csa->jnl;

	if (jpc->qio_active)
	{
		/* Just keep trying */
		if ((status = sys$dclast(ccp_closejnl_ast, db, PSL$C_USER)) != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/

		return;
	}
	assert(0 != jpc->jnllsb->lockid);
	if ((status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0)) != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/
	jpc->jnllsb->lockid = 0;
	if ((status = sys$dassgn(jpc->channel)) != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	jpc->channel = 0;
	jpc->pini_addr = 0;
}
