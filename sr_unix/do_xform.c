/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_string.h"

#include "mdef.h"
#include "gtm_descript.h"
#include "min_max.h"
#include "collseq.h"
#include "do_xform.h"
#include "memprot.h"

error_def(ERR_COLLARGLONG);
error_def(ERR_COLTRANSSTR2LONG);

void do_xform(collseq *csp, int fc_type, mstr *input, mstr *output, int *length)
{
	gtm32_descriptor	outbuff1, insub1;
	gtm_descriptor		outbuff, insub;
	int4			status;
	char			*ba, *addr;
	DEBUG_ONLY(static boolean_t in_do_xform;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!in_do_xform);
	DO_XFORM_RETURN_IF_NULL_STRING(input, output, length);
	DEBUG_ONLY(in_do_xform = TRUE;)
	assert (0 == csp->argtype || 1 == csp->argtype);
	assert(XFORM == fc_type || XBACK == fc_type);
	if (0 == csp->argtype)
	{
		if (MAX_STRLEN_32K < input->len)
		{
			DEBUG_ONLY(in_do_xform = FALSE;)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLARGLONG, 1, csp->act);
		}
		insub.type = DSC_K_DTYPE_T;
		insub.len = input->len;
		insub.val = input->addr;

		outbuff.type = DSC_K_DTYPE_T;
		outbuff.len = MIN(output->len, MAX_STRLEN_32K);
		memprot(&(TREF(protmem_ba)), outbuff.len);
		ba = (TREF(protmem_ba)).addr;
		assert(NULL != ba);
		outbuff.val = (NULL != ba) ? ba : output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub, 1, &outbuff, length);
		else
			status = (csp->xback)(&insub, 1, &outbuff, length);

		if (*length > output->len)
		{
			DEBUG_ONLY(in_do_xform = FALSE;)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLTRANSSTR2LONG, 1, csp->act);
		}
		/* If collation routine has changed outbuff.val (which it will if it cannot store the transformed
		 * result in the buffer that is passed in), the transformed value is stored in the buffer allocated
		 * externally by the collation routine. In this case, update output->addr before returning. */
		addr = (NULL != ba) ? ba : output->addr;
		if (outbuff.val != addr)
			output->addr = outbuff.val;
		else if (NULL != ba)
		{
			assert(*length <= outbuff.len);
			memcpy(output->addr, ba, *length);
		}
	} else
	{
		insub1.type = DSC_K_DTYPE_T;
		insub1.len = input->len;
		insub1.val = input->addr;

		outbuff1.type = DSC_K_DTYPE_T;
		outbuff1.len = output->len;
		memprot(&(TREF(protmem_ba)), outbuff1.len);
		ba = (TREF(protmem_ba)).addr;
		assert(NULL != ba);
		outbuff1.val = (NULL != ba) ? ba : output->addr;

		if (XFORM == fc_type)
			status = (csp->xform)(&insub1, 1, &outbuff1, length);
		else
			status = (csp->xback)(&insub1, 1, &outbuff1, length);

		if (*length > output->len)
		{
			DEBUG_ONLY(in_do_xform = FALSE;)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLTRANSSTR2LONG, 1, csp->act);
		}
		/* If collation routine has changed outbuff1.val (which it will if it cannot store the transformed
		 * result in the buffer that is passed in), the transformed value is stored in the buffer allocated
		 * externally by the collation routine. In this case, update output->addr before returning. */
		addr = (NULL != ba) ? ba : output->addr;
		if (outbuff1.val != addr)
			output->addr = outbuff1.val;
		else if (NULL != ba)
		{
			assert(*length <= outbuff1.len);
			memcpy(output->addr, ba, *length);
		}
	}
	DEBUG_ONLY(in_do_xform = FALSE;)
	if (status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
}
