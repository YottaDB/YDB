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
#include "ccp.h"
#include "locks.h"
#include <lckdef.h>
#include <psldef.h>
#include <ssdef.h>


/* AST routine entered on completion of EX mode lock conversion request
   in ccp_tr_writedb1 (or here) granted (we hope) for got dirty buffers */

void ccp_reqdrtbuf_interrupt( ccp_db_header *db)
{
	uint4		status;
	ccp_action_record	request;


	assert(lib$ast_in_prog());

	if (db->flush_iosb.cond == SS$_NORMAL)
	{
		request.action = CCTR_GOTDRT;
		request.pid = 0;
		request.v.h = db;
		ccp_act_request(&request);
		return;
	}

	ccp_signal_cont(db->flush_iosb.cond);	/***** Is this reasonable? *****/

	if (db->flush_iosb.cond == SS$_DEADLOCK)
	{
		/* Just try again */
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

	return;
}
