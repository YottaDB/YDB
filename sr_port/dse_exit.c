/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */

#include "error.h"
#include "iosp.h"
#include "util.h"
#include "dse_exit.h"

GBLREF	unsigned int		t_tries;

void dse_exit(void)
{
	/* reset t_tries (from CDB_STAGNATE to 0) as we are exiting and no longer going to be running transactions
	 * and an assert in wcs_recover relies on this */
	t_tries = 0;
	util_out_close();
	EXIT(SS_NORMAL);
}
