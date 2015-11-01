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


#include "mlkdef.h"
#include "zshow.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include <varargs.h>
#include "op.h"
#include "mvalconv.h"

void op_gvzwrite(va_alist)
va_dcl
{
	va_list		var;
	bool		flag;
	int4		pat, subsc, count, arg, arg1, arg2;
	mval		*mv;
	zshow_out	output;
	char		buff[MAX_STRLEN];

	memset(&output, 0, sizeof(output));
	output.code = 'V';
	output.type = ZSHOW_DEVICE;
	output.buff = &buff[0];
	output.len = sizeof(buff);
	output.ptr = output.buff;

	VAR_START(var);
	count = va_arg(var, int4);
	pat = va_arg(var, int4);
	subsc = va_arg(var, int4);
	arg1 = va_arg(var, int4);
	gvzwr_init(subsc, (mval *)arg1, pat);
	for (count -=3 ; count > 0;)
	{	mv = va_arg(var, mval *); count--;
		switch ((flag = MV_FORCE_INT(mv)))
		{
		case ZWRITE_ASTERISK:
			gvzwr_arg(flag, 0, 0);  /* caution fall through */
		case ZWRITE_END:
			gvzwr_fini(&output, pat);
			return;
			break;
		case ZWRITE_ALL:
			gvzwr_arg(flag, 0, 0);
			break;
		case ZWRITE_BOTH:
			arg1 = va_arg(var, int4);
			arg2 = va_arg(var, int4);
			count -= 2;
			gvzwr_arg(flag, (mval *)arg1, (mval *)arg2);
			break;
		case ZWRITE_UPPER:
			arg1 = va_arg(var, int4);
			count--;
			gvzwr_arg(flag, (mval *)0, (mval *)arg1);
			break;
		case ZWRITE_VAL:
		case ZWRITE_LOWER:
		case ZWRITE_PATTERN:
			arg1 = va_arg(var, int4);
			count--;
			gvzwr_arg(flag, (mval *)arg1, (mval *)0);
			break;
		default:
			GTMASSERT;
			break;
		}
	}
}
