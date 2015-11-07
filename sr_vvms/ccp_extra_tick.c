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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"

void ccp_extra_tick(ccp_db_header **p)
{
	ccp_db_header *db;

	assert(lib$ast_in_prog());
	db = *p;
	db->extra_tick_done = 1;
	ccp_exitwm_attempt(db);
	return;
}
