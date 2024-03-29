/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for EXIT() */

#ifdef DEBUG
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "gdsfhead.h"
#endif
#include "error.h"
#include "iosp.h"
#include "util.h"
#include "dse_exit.h"
#include "readline.h"

GBLREF	unsigned int		t_tries;
#ifdef DEBUG
GBLREF	sgmnt_addrs		*cs_addrs;
#endif

void dse_exit(void)
{
	/* reset t_tries (from CDB_STAGNATE to 0) as we are exiting and no longer going to be running transactions
	 * and an assert in wcs_recover relies on this */
	t_tries = 0;
	assert((NULL == cs_addrs) || !cs_addrs->now_crit || cs_addrs->hold_onto_crit);
	util_out_close();
	readline_write_history();
	EXIT(SS_NORMAL);
}
