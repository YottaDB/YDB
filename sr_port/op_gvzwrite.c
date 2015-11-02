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

#include "error.h"
#include "mlkdef.h"
#include "zshow.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include <stdarg.h>
#include "op.h"
#include "mvalconv.h"
#include "gtm_maxstr.h"

void op_gvzwrite(UNIX_ONLY_COMMA(int4 count) int4 pat, ...)
{
	va_list		var;
	bool		flag;
	int4		subsc, arg;
	INTPTR_T	arg1, arg2;
	VMS_ONLY(int4	count;)
	mval		*mv;
	zshow_out	output;
	DCL_THREADGBL_ACCESS;
	MAXSTR_BUFF_DECL(buff);

	SETUP_THREADGBL_ACCESS;
	TREF(in_zwrite) = TRUE;
	MAXSTR_BUFF_INIT;
	memset(&output, 0, SIZEOF(output));
	output.code = 'V';
	output.type = ZSHOW_DEVICE;
	output.buff = &buff[0];
	output.size = SIZEOF(buff);
	output.ptr = output.buff;

	VAR_START(var, pat);
	VMS_ONLY(va_count(count);)
	subsc = va_arg(var, int4);
	arg1 = va_arg(var, INTPTR_T);
	gvzwr_init(subsc, (mval *)arg1, pat);
	for (count -=3 ; count > 0;)
	{	mv = va_arg(var, mval *); count--;
		switch ((flag = MV_FORCE_INT(mv)))
		{
		case ZWRITE_ASTERISK:
			gvzwr_arg(flag, 0, 0);  /* caution fall through */
		case ZWRITE_END:
			gvzwr_fini(&output, pat);
			MAXSTR_BUFF_FINI;
			TREF(in_zwrite) = FALSE;
			return;
			break;
		case ZWRITE_ALL:
			gvzwr_arg(flag, 0, 0);
			break;
		case ZWRITE_BOTH:
			arg1 = va_arg(var, INTPTR_T);
			arg2 = va_arg(var, INTPTR_T);
			count -= 2;
			gvzwr_arg(flag, (mval *)arg1, (mval *)arg2);
			break;
		case ZWRITE_UPPER:
			arg1 = va_arg(var, INTPTR_T);
			count--;
			gvzwr_arg(flag, (mval *)0, (mval *)arg1);
			break;
		case ZWRITE_VAL:
		case ZWRITE_LOWER:
		case ZWRITE_PATTERN:
			arg1 = va_arg(var, INTPTR_T);
			count--;
			gvzwr_arg(flag, (mval *)arg1, (mval *)0);
			break;
		default:
			GTMASSERT;
			break;
		}
	}
	va_end(var);
	MAXSTR_BUFF_FINI;
	TREF(in_zwrite) = FALSE;
}
