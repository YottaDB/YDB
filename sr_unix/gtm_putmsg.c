/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "util.h"
#include "gtmmsg.h"
#include "gtm_putmsg_list.h"
#include "gtmimagename.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs		jnlpool;

/*  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 */

void gtm_putmsg(int argcnt, ...)
{
	va_list	var;
	sgmnt_addrs	*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = (ANTICIPATORY_FREEZE_AVAILABLE && jnlpool.jnlpool_ctl) ? REG2CSA(gv_cur_region) : NULL;
	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	util_out_print("",TRUE);
}

void gtm_putmsg_csa(void *csa, int argcnt, ...)
{
	va_list	var;

	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	util_out_print("",TRUE);
}

void gtm_putmsg_noflush(int argcnt, ...)
{
	va_list var;
	sgmnt_addrs	*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = (ANTICIPATORY_FREEZE_AVAILABLE && jnlpool.jnlpool_ctl) ? REG2CSA(gv_cur_region) : NULL;
	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
}

void gtm_putmsg_noflush_csa(void *csa, int argcnt, ...)
{
	va_list var;

	VAR_START(var, argcnt);
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
}
