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

GBLREF stack_frame	*frame_pointer;

unsigned char *get_symb_line (unsigned char *out, unsigned char **b_line, unsigned char **ctxt)
{
	bool		line_reset;
	stack_frame	*fp;
	unsigned char	*addr, *out_addr;

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		if ((unsigned char *) fp->rvector + fp->rvector->ptext_ptr <= fp->mpc &&
			fp->mpc < (unsigned char *) fp->rvector + fp->rvector->vartab_ptr)
		{
			if (ctxt != 0) *ctxt = fp->ctxt;
			if (line_reset)
			{	addr = fp->mpc + 1;
			}
			/* this may be dead code: examine relation with if above */
			else if ((unsigned char*) fp->rvector + fp->rvector->current_rhead_ptr == fp->mpc)
			{	addr = fp->mpc + 1;
			}
			else
			{	addr = fp->mpc;
			}
			out_addr = symb_line (addr, out, b_line, fp->rvector);
			assert (out < out_addr);
			return out_addr;
		}
		else
		{
			if (fp->type & SFT_ZTRAP || fp->type & SFT_DEV_ACT)
			{	line_reset = TRUE;
			}
		}
	}
	GTMASSERT;
}
