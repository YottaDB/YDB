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
#include <psldef.h>
#include <ssdef.h>


/* Request to relinquish write mode, implying that there is a write mode request from another machine */

void ccp_tr_exwmreq( ccp_action_record	*rec)
{
	ccp_db_header	*db;
	uint4	status;


	db = rec->v.h;

	if (db != NULL  &&  !db->wmexit_requested)
	{
		status = sys$dclast(ccp_exitwm_blkast, &db->exitwm_timer_id, PSL$C_USER);
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	return;
}
