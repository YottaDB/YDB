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
#include <psldef.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "ccpact.h"


static	void	ccp_closejnl_ast_user( gd_region *reg)
{
	ccp_action_record	buffer;

	buffer.action = CCTR_CLOSEJNL;
	buffer.pid = 0;
	buffer.v.reg = reg;
	ccp_act_request(&buffer);
}


/* NOTE:  Because this blocking AST routine is established via a call to gtm_enqw, it
   executes in KERNEL mode;  ccp_closejnl_ast_user, however, must execute in USER mode.
   This is accomplished by using sys$dclast, explicitly specifying USER mode. */

void	ccp_closejnl_ast( gd_region	*reg)
{
	assert(lib$ast_in_prog());

	sys$dclast(ccp_closejnl_ast_user, reg, PSL$C_USER);
}
