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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "locks.h"
#include <psldef.h>
#include <lckdef.h>
#include <ssdef.h>


/* Request to wake up all cluster processes waiting for an M-lock */

void ccp_tr_lkrqwake( ccp_action_record	*rec)
{
	ccp_db_header	*db;
	uint4	status;


	db = ccp_get_reg(&rec->v.file_id);
	if (db != NULL)
	{
		/* Deliver blocking AST's to any CCP's running on other nodes */
		status = ccp_enq(0, LCK$K_EXMODE, &db->lock_iosb, LCK$M_CONVERT, NULL, 0,
				 NULL, 0, NULL, PSL$C_USER, 0);
		/***** Check error status here? *****/

		/* Cancel the previous request;  its only purpose was to deliver the blocking AST's */
		status = sys$deq(db->lock_iosb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
		if (status != SS$_NORMAL  &&  status != SS$_CANCELGRANT)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/

		/* Reestablish our own blocking AST routine */
		status = ccp_enq(0, LCK$K_CRMODE, &db->lock_iosb, LCK$M_CONVERT | LCK$M_SYNCSTS, NULL, 0,
				 ccp_lkrqwake1, db, ccp_lkdowake_blkast, PSL$C_USER, 0);
		/***** Check error status here? *****/
	}

	return;
}
