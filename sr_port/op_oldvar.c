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
#include "mv_stent.h"
#include "op.h"

GBLREF mv_stent *mv_chain;
GBLREF unsigned char *msp;

void op_oldvar(void)
{
	assert(msp == (unsigned char *)mv_chain);
	unw_mv_ent(mv_chain);
	POP_MV_STENT();
	return;
}
