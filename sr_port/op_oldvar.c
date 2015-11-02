/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "mv_stent.h"
#include "op.h"
#include "error.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp;

void op_oldvar(void)
{
	assert(msp == (unsigned char *)mv_chain);
	assert(MVST_TRIGR != mv_chain->mv_st_type);	/* Should never unwind a trigger frame here */
	DBGEHND((stderr, "op_oldvar: Popping mv_stent at addr 0x"lvaddr" with type %d\n", mv_chain, mv_chain->mv_st_type));
	POP_MV_STENT();
	return;
}
