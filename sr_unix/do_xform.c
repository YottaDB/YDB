/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_descript.h"
#include "min_max.h"
#include "collseq.h"
#include "do_xform.h"

void do_xform(collseq *csp, int fc_type, mstr *input, mstr *output, int *length)
{
	gtm32_descriptor outbuff1, insub1;
	gtm_descriptor outbuff, insub;
	int4	status;

	error_def(ERR_COLLARGLONG);

	assert (0 == csp->argtype || 1 == csp->argtype);
	assert(XFORM == fc_type || XBACK == fc_type);
	if (0 == csp->argtype)
	{
		if (MAX_STRLEN_32K < input->len)
			rts_error(VARLSTCNT(3) ERR_COLLARGLONG, 1, csp->act);
		insub.type = DSC_K_DTYPE_T;
		insub.len = input->len;
		insub.val = input->addr;

		outbuff.type = DSC_K_DTYPE_T;
		outbuff.len = MIN(output->len, MAX_STRLEN_32K);
		outbuff.val = output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub, 1, &outbuff, length);
		else
			status = (csp->xback)(&insub, 1, &outbuff, length);

	} else
	{
		insub1.type = DSC_K_DTYPE_T;
		insub1.len = input->len;
		insub1.val = input->addr;

		outbuff1.type = DSC_K_DTYPE_T;
		outbuff1.len = output->len;
		outbuff1.val = output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub1, 1, &outbuff1, length);
		else
			status = (csp->xback)(&insub1, 1, &outbuff1, length);
	}
	if (status)
			rts_error(VARLSTCNT(1) status);
}
