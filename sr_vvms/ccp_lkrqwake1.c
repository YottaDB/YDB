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


/* AST routine entered on completion of CR mode lock conversion request
   in ccp_opendb3c, ccp_tr_lkrqwake, ccp_tr_writedb1, or here */

void ccp_lkrqwake1( ccp_db_header *db)
{
	uint4	status;


        assert(lib$ast_in_prog());

	if (db->lock_iosb.cond == SS$_NORMAL)
		return;

	ccp_signal_cont(db->lock_iosb.cond);	/***** Is this reasonable? *****/

	if (db->lock_iosb.cond == SS$_DEADLOCK)
	{
		/* Just try again */
		status = ccp_enq(0, LCK$K_CRMODE, &db->lock_iosb, LCK$M_CONVERT | LCK$M_SYNCSTS, NULL, 0,
				 ccp_lkrqwake1, db, ccp_lkdowake_blkast, PSL$C_USER, 0);
		/***** Check error status here? *****/
	}

	return;
}
