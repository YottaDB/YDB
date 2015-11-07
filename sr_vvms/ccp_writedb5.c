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
#include "ccpact.h"

void ccp_writedb5(ccp_db_header *db)
{
	ccp_action_record buff;

	buff.action = CCTR_WRITEDB1;
	buff.pid = 0;
	buff.v.h = db;
	ccp_act_request(&buff);
	return;
}
