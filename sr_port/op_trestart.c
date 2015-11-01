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
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "cdb_sc.h"
#include "hashtab.h"		/* needed for tp.h */
#include "jnl.h"
#include "tp.h"
#include "tp_restart.h"
#include "op.h"

GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;

void	op_trestart(int newlevel)
{
	error_def(ERR_TPRETRY);

	t_tries = 0;				/* forgive any trespass */
	t_fail_hist[t_tries] = cdb_sc_normal;
	assert(1 == newlevel);	/* newlevel probably needs to become GBLREF assigned here and reset to 1 in tp_restart */
	INVOKE_RESTART;
}
