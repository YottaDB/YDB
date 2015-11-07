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


/* AST routine entered on completion of sys$qio to write the lock block, in ccp_tr_exitwm or ccp_exitwm1 */

void ccp_exitwm1a( ccp_db_header *db)
{
	assert(lib$ast_in_prog());

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	ccp_exitwm2(db);

	return;
}
