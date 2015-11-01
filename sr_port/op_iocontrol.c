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

#include "gtm_string.h"

#include "stringpool.h"
#include "io.h"
#include <varargs.h>
#include "op.h"

/* THIS VERSION OF OP_IOCONTROL FORMATS MNENOMIC SPACE COMMANDS IN A
   'SOURCE' FORMAT....THIS IS FOR UNIV OF LOWELL TEST ONLY
*/

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

void op_iocontrol(va_alist)
va_dcl
{
	va_list var, sav_var;
	mval *vp;
	int n, m;
	int count;
	unsigned char *cp, *cq;
	int length;
	mstr val;
	error_def(ERR_CTLMNEMAXLEN);

	VAR_START(var);
	n = va_arg(var, int4);
	VAR_COPY(sav_var, var);
	for (count = 0 ; count < n ; count++)
	{
		vp = va_arg(var, mval *);
		MV_FORCE_STR(vp);
	}
	VAR_COPY(var, sav_var);
	/* format will be:
		if n=1  KEYWORD
		if n>1  KEYWORD(PAR1,PAR2,...PARx)
	*/
	if (stringpool.top - stringpool.free < MAX_DEVCTL_LENGTH + 3)
		stp_gcol(MAX_DEVCTL_LENGTH + 3);
	for (cp = stringpool.free, count = 0 ; count < n ; count++)
	{	vp = va_arg(var, mval *);
		if (count > 0)
			*cp++ = (count == 1) ? '(' : ',';
		if (MV_IS_CANONICAL(vp))
		{
			m = vp->str.len;
			if (cp + m > stringpool.top)
				break;
			memcpy(cp, vp->str.addr, m);
			cp += m;
		}
		else
		{
			if (count > 0)
				*cp++ = '"';
			for (m = 0, cq = (unsigned char *)vp->str.addr, length = vp->str.len
				; m < length && cp < stringpool.top ; m++)
			{
				if (*cq == '"')
					*cp++ = '"';
				*cp++ = *cq++;
			}
			if (cp > stringpool.top)
				break;
			if (count > 0)
				*cp++ = '"';
		}
	}
	if (count > 1)
		*cp++ = ')';
	if (cp - stringpool.free > MAX_DEVCTL_LENGTH)
		rts_error(VARLSTCNT(1) ERR_CTLMNEMAXLEN);
	assert(cp < stringpool.top);
	val.len = cp - stringpool.free;
	val.addr = (char *)stringpool.free;
	(*io_curr_device.out->disp_ptr->iocontrol)(&val);
	return;
}
