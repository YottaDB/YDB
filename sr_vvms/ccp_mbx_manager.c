/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include <iodef.h>
#include <efndef.h>


GBLREF	unsigned short	ccp_channel;
static bool mailbox_running = FALSE;

static void ccp_mbx_interrupt(void);

void ccp_mbx_start(void)
{
	uint4 status;
	error_def(ERR_CCPMBX);

	if (mailbox_running)
		return;
	mailbox_running = TRUE;
	status = sys$qio(0, ccp_channel, (IO$_SETMODE|IO$M_WRTATTN), 0,0,0, ccp_mbx_interrupt,0 ,0 ,0 ,0 ,0);
	if ((status & 1) == 0)
		lib$signal(ERR_CCPMBX, 0 , status);
	return;
}

static void ccp_mbx_interrupt(void)
{
	ccp_action_record buff;
	unsigned short mbsb[4];
	bool more_room;
	uint4 status;
	error_def(ERR_CCPMBX);

	mailbox_running = FALSE;
	status = sys$qiow(EFN$C_ENF, ccp_channel, (IO$_READVBLK | IO$M_NOW), &mbsb[0], 0, 0, &buff,
						SIZEOF(buff), 0, 0, 0, 0);
	if (status & 1)
		status = mbsb[0];
	if ((status & 1) == 0)
		lib$signal(ERR_CCPMBX, 0, status);
	more_room = ccp_act_request(&buff);
	if (more_room)
		ccp_mbx_start();
	return;
}
