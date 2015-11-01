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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "gtmci.h"

GBLREF  stack_frame     	*frame_pointer;
GBLREF  mv_stent                *mv_chain;

/* remove last n mvals that have been pushed into the current frame */
void unw_mv_ent_n(int n)
{
	mv_stent        *mvc;
	int		argn = 0;
	assert(n >= 0);
        for (mvc = mv_chain; argn < n && mvc < (mv_stent *)frame_pointer; ++argn)
        {
                unw_mv_ent(mvc);
                mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
        }
	mv_chain = mvc;
}
