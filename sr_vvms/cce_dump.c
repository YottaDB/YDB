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
#include "cce_output.h"

void cce_dump(void)
{
	ccp_action_aux_value mbxname;

	cce_get_return_channel(&mbxname);
	ccp_sendmsg(CCTR_QUEDUMP, &mbxname);
	cce_read_return_channel();
	return;
}
