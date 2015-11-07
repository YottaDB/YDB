/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "get_page_size.h"
#include "gtm_env_init.h"     /* for gtm_env_init() prototype */

GBLDEF	bool		ccp_stop;
GBLDEF	int		ccp_stop_ctr;
GBLREF	bool		licensed ;
int ccp(void)
{
	ccp_action_record	*act;
	error_def(ERR_CCPBADMSG);
	error_def(ERR_OPRCCPSTOP);

#define CCP_TABLE_ENTRY(A,B,C,D) void B();
#include "ccpact_tab.h"
#undef CCP_TABLE_ENTRY
#define CCP_TABLE_ENTRY(A,B,C,D) B,
	static readonly (*dispatch_table[])() =
{
#include "ccpact_tab.h"
};
#undef CCP_TABLE_ENTRY

	licensed= TRUE ;
	gtm_env_init();	/* read in all environment variables before any function call (particularly malloc) */
	get_page_size();
	ccp_init();
	lib$establish(ccp_ch);
	while (!ccp_stop || ccp_stop_ctr > 0)
	{
		act = ccp_act_select();
		if (!act)
			sys$hiber();
		else
		{
			if (act->action < 0 || act->action >= CCPACTION_COUNT)
				ccp_signal_cont(ERR_CCPBADMSG);
			else
				(*dispatch_table[act->action])(act);
			ccp_act_complete();
		}
	}
	ccp_exit();
	return ERR_OPRCCPSTOP;
}
