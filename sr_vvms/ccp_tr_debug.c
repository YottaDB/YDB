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
#include <ssdef.h>

void ccp_tr_debug(r)
ccp_action_record *r;
{
	/* could/should establish dbg$input and dbg$output */
	lib$signal(SS$_DEBUG);
	return;
}
