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

#include "gtm_string.h"

#include "stringpool.h"
#include "io.h"
#include <stdarg.h>
#include "op.h"

#define LIMITCHECK(x, xmax)		\
	if ((x) > (xmax))		\
	{				\
		overflow = TRUE;	\
		break;			\
	}

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

error_def(ERR_CTLMNEMAXLEN);

void op_iocontrol(UNIX_ONLY_COMMA(int4 n) mval *vparg, ...)
{
	va_list		var;
	mval		*vp;
	VMS_ONLY(int	n;)
	int		count, m;
	unsigned char	*cp, *cq, *cpmax;
	boolean_t	overflow;
	int		length;
	mstr		val;

	VAR_START(var, vparg);
	VMS_ONLY(va_count(n);)
	assert(0 < n);
	MV_FORCE_STR(vparg);
	for (count = 1; count < n; count++)
	{
		vp = va_arg(var, mval *);
		MV_FORCE_STR(vp);
	}
	va_end(var);
	/* Format of generated argument is:
	 *	if n=1  KEYWORD
	 *	if n>1  KEYWORD(PAR1,PAR2,...PARx)
	 */
	VAR_START(var, vparg);
	vp = vparg;
	ENSURE_STP_FREE_SPACE(MAX_DEVCTL_LENGTH + 1);		/* Plus 1 to allow for null terminator char sockets need */
	/* Note in this calculation, cpmax is the true maximum value for cp so cp can be equal to but not greater than cpmax */
	cpmax = stringpool.free + MAX_DEVCTL_LENGTH;
	overflow = FALSE;
	for (cp = stringpool.free, count = 0 ; count < n; count++)
	{
		if (0 < count)
		{
			vp = va_arg(var, mval *);
			*cp++ = (1 == count) ? '(' : ',';
			LIMITCHECK(cp, cpmax);
		}
		if (MV_IS_CANONICAL(vp))
		{
			m = (int)vp->str.len;
			LIMITCHECK((cp + m), cpmax);		/* Check before move for multiple chars */
			memcpy(cp, vp->str.addr, m);
			cp += m;
		} else
		{
			if (0 < count)
			{
				*cp++ = '"';
				LIMITCHECK(cp, cpmax);
			}
			for (m = 0, cq = (unsigned char *)vp->str.addr, length = (int)vp->str.len; m < length; m++)
			{
				if ('"' == *cq)
				{
					*cp++ = '"';
					LIMITCHECK(cp, cpmax);
				}
				*cp++ = *cq++;
				LIMITCHECK(cp, cpmax);
			}
			if (0 < count)
			{
				*cp++ = '"';
				LIMITCHECK(cp, cpmax);
			}
		}
	}
	va_end(var);
	if ((1 < count) && !overflow)
	{
		*cp++ = ')';
		if (cp > cpmax)
			overflow = TRUE;
	}
	if (overflow)
		rts_error(VARLSTCNT(1) ERR_CTLMNEMAXLEN);
	assert(cp <= cpmax);
	val.len = INTCAST(cp - stringpool.free);
	assert(val.len <= MAX_DEVCTL_LENGTH);
	val.addr = (char *)stringpool.free;
	(*io_curr_device.out->disp_ptr->iocontrol)(&val);
	return;
}
