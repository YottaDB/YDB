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
#include "error.h"
#include "gdsroot.h"
#include "ccp.h"
#include "ccpact.h"

typedef struct{
int4	link;
int4	*exit_hand;
int4	arg_cnt;
int4	*cond_val;
}desblk;

GBLREF int4 ccp_exi_condition;
GBLREF desblk ccp_exi_blk;
GBLDEF bool ccp_dump_on;

void ccp_rundown(void)
{
	error_def(ERR_CCEDUMPON);
	error_def(ERR_CCEDUMPOFF);
	error_def(ERR_CCEDUMPNOW);
	error_def(ERR_FORCEDHALT);

	ccp_action_record buff;

	if (ccp_exi_condition == ERR_CCEDUMPNOW || ccp_exi_condition == ERR_CCEDUMPON ||
		 ccp_exi_condition == ERR_CCEDUMPOFF || ccp_exi_condition == ERR_FORCEDHALT)
	{	if (ccp_exi_condition == ERR_FORCEDHALT)
		{	buff.action = CCTR_STOP;
			buff.pid=0;
			ccp_priority_request(&buff);
		}
		else
		{	if (ccp_exi_condition == ERR_CCEDUMPNOW)
				ccp_dump();
			else
				ccp_dump_on = (ccp_exi_condition == ERR_CCEDUMPON);
		}
		ccp_exi_blk.exit_hand = &ccp_rundown;
		ccp_exi_blk.arg_cnt = 1;
		ccp_exi_blk.cond_val = &ccp_exi_condition;
		sys$dclexh(&ccp_exi_blk);
		lib$establish(ccp_exi_ch);
		lib$signal(ccp_exi_condition);	/* signal an error so can unwind and continue processing */
	}
	lib$signal(ccp_exi_condition);
}
