/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "error_trap.h"		/* for error_ret() declaration */
#include "gtmimagename.h"

#define PROCESS_EXITING		"-EXITING"

GBLREF stack_frame		*frame_pointer;
GBLREF stack_frame		*error_frame;
GBLREF dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */
GBLREF int			process_exiting;

LITREF gtmImageName		gtmImageNames[];

unsigned char *get_symb_line(unsigned char *out, unsigned char **b_line, unsigned char **ctxt)
{
	boolean_t	line_reset;
	stack_frame	*fp;
	unsigned char	*addr, *out_addr;
	unsigned char	*fpmpc, *fpctxt;

	line_reset = FALSE;
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
#		ifdef GTM_TRIGGER
		if ((NULL == fp->old_frame_pointer) && (fp->type & SFT_TRIGR))
			/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
			fp = *(stack_frame **)(fp + 1);
#		endif
		fpmpc = fp->mpc;
		fpctxt = fp->ctxt;
		if (ADDR_IN_CODE(fpmpc, fp->rvector))
		{
			if (NULL != ctxt)
				*ctxt = fpctxt;
			if (line_reset)
				addr = fpmpc + 1;
			else
				addr = fpmpc;
			out_addr = symb_line(addr, out, b_line, fp->rvector);
			assert(out < out_addr);
			return out_addr;
		} else
		{
			if ((fp->type & SFT_ZTRAP) || (fp->type & SFT_DEV_ACT))
				line_reset = TRUE;
		}
	}
	/* At this point, we were unable to discover what was executing from the M stack. It is possible this error
	 * occurred either while GT.M was shutting down thus the stack could be completely unwound. Or this may not
	 * even be a mumps process (could be update process driving a trigger). Or this could have nothing to do with
	 * generated code and could occur in most any of the utility modules - especially replication processes. Do
	 * what we can to indicate where the problem occurred.
	 */
	if (process_exiting || !IS_MCODE_RUNNING)
	{	/* Show which image is running */
		memcpy(out, gtmImageNames[image_type].imageName, gtmImageNames[image_type].imageNameLen);
		out_addr = out + gtmImageNames[image_type].imageNameLen;
		if (process_exiting)
		{	/* Add the information that this process was exiting */
			MEMCPY_LIT(out_addr, PROCESS_EXITING);
			out_addr += STR_LIT_LEN(PROCESS_EXITING);
		}
		return out_addr;
	}
	/* At this point we know the process was not exiting, was executing M code, but we *still* couldn't find where
	 * on the M stack. This is an out of design situation so we cause an assert failure.
	 */
	assertpro(fp);
	return NULL;	/* For the compiler */
}
