/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*error_last_mpc_err;
GBLREF unsigned char	*error_last_ctxt_err;
GBLREF stack_frame	*error_last_frame_err;

unsigned char *get_symb_line (unsigned char *out, unsigned char **b_line, unsigned char **ctxt)
{
	bool		line_reset;
	stack_frame	*fp;
	unsigned char	*addr, *out_addr;
	unsigned char	*fpmpc, *fpctxt;

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		fpmpc = (fp == error_last_frame_err) ? error_last_mpc_err : fp->mpc;
		fpctxt = (fp == error_last_frame_err) ? error_last_ctxt_err : fp->ctxt;
		/*The equality check in the second half of the following expression is to
		  account for the delay-slot in HP-UX for implicit quits. Not an issue here,
		  but added for uniformity. */
		if ((unsigned char *) fp->rvector + fp->rvector->ptext_ptr <= fpmpc &&
			fpmpc <= (unsigned char *) fp->rvector + fp->rvector->vartab_ptr)
		{
			if (ctxt != 0)
				*ctxt = fpctxt;
			if (line_reset)
				addr = fpmpc + 1;
			else if ((unsigned char*) fp->rvector + fp->rvector->current_rhead_ptr == fpmpc)
				addr = fpmpc + 1;	/* this may be dead code: examine relation with if above */
			else
				addr = fpmpc;
			out_addr = symb_line (addr, out, b_line, fp->rvector);
			assert (out < out_addr);
			return out_addr;
		} else
		{
			if (fp->type & SFT_ZTRAP || fp->type & SFT_DEV_ACT)
				line_reset = TRUE;
		}
	}
	GTMASSERT;
}
