/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <climsgdef.h>
#include <descrip.h>
#include <rmsdef.h>

#include "gdsroot.h"
#include "ccp.h"
#include "ccpact.h"
#include "getjobnum.h"
#include "dfntmpmbx.h"
#include "get_page_size.h"
#include "gtm_env_init.h"     /* for gtm_env_init() prototype */
#include "gtm_threadgbl_init.h"

GBLDEF	uint4	ret_status = 1;

static bool cce_done;

cce()
{
	$DESCRIPTOR(prompt,"CCE> ");
	int status;
	char buff[512];
	$DESCRIPTOR(command,buff);
	unsigned short outlen;
	mstr	lnm$group = {9, "LNM$GROUP"};
	int CCE_CMD(), CLI$DCL_PARSE(), CLI$DISPATCH();
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;			/* This is the first C routine in the cce so do init here */
	gtm_env_init();	/* read in all environment variables before any function call (particularly malloc) */
	getjobnum();
	get_page_size();
	dfntmpmbx (lnm$group.len, lnm$group.addr);
	status = lib$get_foreign(&command,0,&outlen,0);
	if ((status & 1) && outlen > 0)
	{	command.dsc$w_length = outlen;
		status = CLI$DCL_PARSE(&command ,&CCE_CMD, &lib$get_input, 0, 0);
		if (status == CLI$_NORMAL)
			CLI$DISPATCH();
	}else
	{	while (!cce_done)
		{
			status = CLI$DCL_PARSE(0 ,&CCE_CMD, &lib$get_input
			, &lib$get_input, &prompt);
			if (status == RMS$_EOF)
				cce_done = TRUE;
			else if (status == CLI$_NORMAL)
			     {
				ret_status = 1;
				CLI$DISPATCH();
			     }
		}
	}
	return ret_status;
}

void cce_stop()
{

	ccp_sendmsg(CCTR_STOP, 0);
	return;
}

void cce_debug()
{

	ccp_sendmsg(CCTR_DEBUG, 0);
	return;
}

void cce_exit()
{
	cce_done = TRUE;
	return;
}
