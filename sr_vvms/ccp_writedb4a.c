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
#include <ssdef.h>


/* AST routine entered on completion of sys$qio to read the lock block,
   in ccp_writedb2, ccp_writedb3, or ccp_writedb4 */

void ccp_writedb4a(ccp_db_header *db)
{
	assert(lib$ast_in_prog());

	if (db->segment == NULL  ||  db->segment->nl->ccp_state == CCST_CLOSED)
		return;

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	ccp_writedb5(db);

	return;
}
