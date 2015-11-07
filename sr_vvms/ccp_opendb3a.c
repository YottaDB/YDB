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
#include "ccpact.h"
#include "ccp_opendb3a.h"

void ccp_opendb3a( ccp_db_header *db)
{
	ccp_action_record buff;

	if ((db->wm_iosb.cond & 1) == 0)
		lib$signal(db->wm_iosb.cond);
	buff.action = CCTR_OPENDB3A;
	buff.pid = 0;
	buff.v.h = db;
	ccp_act_request(&buff);
	return;
}
