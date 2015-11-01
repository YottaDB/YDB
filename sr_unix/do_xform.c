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
#include "gtm_descript.h"
#include "do_xform.h"

void do_xform( int4 (*xfm)(), mstr *input, mstr *output, int *length)
{
	gtm_descriptor outbuff, insub;
	int4	status;

	insub.type = DSC_K_DTYPE_T;
	insub.len = input->len;
	insub.val = input->addr;

	outbuff.type = DSC_K_DTYPE_T;
	outbuff.len = output->len;
	outbuff.val = output->addr;

	status = (xfm)(&insub, 1, &outbuff, length);
	if (status)
		rts_error(VARLSTCNT(1) status);
}
