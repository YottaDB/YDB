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


/* AST routine entered on completion of sys$qio to read transaction history in ccp_opendb2 */

void ccp_opendb3( ccp_db_header	*db)
{
	ccp_action_record	request;


	assert(lib$ast_in_prog());

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	request.action = CCTR_OPENDB3;
	request.pid = 0;
	request.v.h = db;
	ccp_act_request(&request);

	return;
}
