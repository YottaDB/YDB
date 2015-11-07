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

#include <descrip.h>

#include "collseq.h"
#include "do_xform.h"

#define	MAX_INPUT_LEN 65535

void do_xform(collseq *csp, int fc_type, mstr *input, mstr *output, int *length)
{
	struct dsc64$descriptor outbuff1, insub1;
	struct dsc$descriptor outbuff, insub;
	int4	status;

	error_def(ERR_COLLARGLONG);

	DO_XFORM_RETURN_IF_NULL_STRING(input, output, length);
	assert (0 == csp->argtype || 1 == csp->argtype);
	assert(XFORM == fc_type || XBACK == fc_type);
	if (0 == csp->argtype)
	{
		if (MAX_INPUT_LEN < input->len)
			rts_error(VARLSTCNT(3) ERR_COLLARGLONG, 1, csp->act);
		insub.dsc$b_dtype = DSC$K_DTYPE_T;
		insub.dsc$b_class = DSC$K_CLASS_S;
		insub.dsc$w_length = input->len;
		insub.dsc$a_pointer = input->addr;

		outbuff.dsc$b_dtype = DSC$K_DTYPE_T;
		outbuff.dsc$b_class = DSC$K_CLASS_S;
		outbuff.dsc$w_length = output->len;
		outbuff.dsc$a_pointer = output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub, 1, &outbuff, length);
		else
			status = (csp->xback)(&insub, 1, &outbuff, length);
		/* If collation routine has changed outbuff1.val, it stores the transformed value in the
		 * externally allocated buffer. In this case, update output->addr before returning. */
		if (outbuff.dsc$a_pointer != output->addr)
			output->addr = outbuff.dsc$a_pointer;
	} else
	{
		insub1.dsc64$b_dtype = DSC64$K_DTYPE_T;
		insub1.dsc64$b_class = DSC64$K_CLASS_S;
		insub1.dsc64$q_length = input->len;
		insub1.dsc64$pq_pointer = input->addr;

		outbuff1.dsc64$b_dtype = DSC64$K_DTYPE_T;
		outbuff1.dsc64$b_class = DSC64$K_CLASS_S;
		outbuff1.dsc64$q_length = output->len;
		outbuff1.dsc64$pq_pointer = output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub1, 1, &outbuff1, length);
		else
			status = (csp->xback)(&insub1, 1, &outbuff1, length);
		/* If collation routine has changed outbuff1.val, it stores the transformed value in the
		 * externally allocated buffer. In this case, update output->addr before returning. */
		if (outbuff1.dsc64$pq_pointer != output->addr)
		{
			output->addr = (char*)(outbuff1.dsc64$pq_pointer);
			assert(output->addr == outbuff1.dsc64$pq_pointer); /* ensure 32-bit address */
		}
	}
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
}
