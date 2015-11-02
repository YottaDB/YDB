/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "error_trap.h"		/* for error_ret()/error_ret_vms() declaration */

GBLREF stack_frame		*frame_pointer;
GBLREF stack_frame		*error_frame;
GBLREF dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */

unsigned char *get_symb_line (unsigned char *out, unsigned char **b_line, unsigned char **ctxt)
{
	bool		line_reset;
	stack_frame	*fp;
	unsigned char	*addr, *out_addr;
	unsigned char	*fpmpc, *fpctxt;

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		fpmpc = ((fp != error_frame) || (fp->mpc != dollar_ecode.error_rtn_addr))
				? fp->mpc : dollar_ecode.error_frame_mpc;
		fpctxt = ((fp != error_frame) || (fp->ctxt != dollar_ecode.error_rtn_ctxt))
				? fp->ctxt : dollar_ecode.error_frame_ctxt;
		if (ADDR_IN_CODE(fpmpc, fp->rvector))
		{
			if (ctxt != 0)
				*ctxt = fpctxt;
			if (line_reset)
				addr = fpmpc + 1;
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
	return NULL;
}
