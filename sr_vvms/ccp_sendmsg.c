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
#include <ssdef.h>
#include "gdsroot.h"
#include "ccp.h"
#include <iodef.h>
#include <descrip.h>
#include <psldef.h>

error_def(ERR_CCPMBX);

GBLREF	uint4	process_id;

static	short		channel;
static	ccp_iosb	iosb;
static	$DESCRIPTOR	(devnam, CCP_MBX_NAME);


short	ccp_sendmsg(action, aux_value)
ccp_action_code		action;
ccp_action_aux_value	*aux_value;
{
	ccp_action_record	request;


	if (channel == 0  &&  sys$assign(&devnam, &channel, PSL$C_USER, NULL) != SS$_NORMAL)
	{
		rts_error(ERR_CCPMBX);
		return 0;	/* Necessary for operation of cce_ccp */
	}

	request.action = action;
	request.pid = process_id;
	if (aux_value != NULL)
		request.v = *aux_value;

	if (sys$qio(0, channel, IO$_WRITEVBLK | IO$M_NOW, &iosb, NULL, 0, &request, sizeof request, 0, 0, 0, 0)
		!= SS$_NORMAL)
	{
		rts_error(ERR_CCPMBX);
		return 0;		/* Necessary for operation of cce_ccp */
	}

	return channel;
}
