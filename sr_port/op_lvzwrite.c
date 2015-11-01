/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "op.h"
#include "mvalconv.h"
#include <stdarg.h>
#include "gtm_maxstr.h"

void op_lvzwrite(UNIX_ONLY_COMMA(int4 count) int4 arg1, ...)
{
	va_list		var;
	boolean_t	flag;
	int4		arg2;
	VMS_ONLY(int4	count;)
	mval		*mv;
	zshow_out	output, *out;
	MAXSTR_BUFF_DECL(buff);

	VAR_START(var, arg1);
	VMS_ONLY(va_count(count);)
	MAXSTR_BUFF_INIT;
	memset(&output, 0, sizeof(output));
	output.code = 'V';
	output.type = ZSHOW_DEVICE;
	output.buff = &buff[0];
	output.size = sizeof(buff);
	output.ptr = output.buff;
	out = &output;
	count--;
	lvzwr_init(FALSE, (mval *)arg1);
	for (; count > 0; )
	{	mv = va_arg(var, mval *); count--;
		switch ((flag = MV_FORCE_INT(mv)))
		{
		case ZWRITE_ASTERISK:
			lvzwr_arg(flag, (mval *)0, (mval *)0); /* caution fall through */
		case ZWRITE_END:
			lvzwr_fini(out, flag);
			MAXSTR_BUFF_FINI;
			return;
			break;
		case ZWRITE_ALL:
			lvzwr_arg(flag, (mval *)0, (mval *)0);
			break;
		case ZWRITE_BOTH:
			arg1 = va_arg(var, int4);
			arg2 = va_arg(var, int4);
			count -= 2;
			lvzwr_arg(flag, (mval *)arg1, (mval *)arg2);
			break;
		case ZWRITE_UPPER:
			arg1 = va_arg(var, int4);
			count--;
			lvzwr_arg(flag, (mval *)0, (mval *)arg1);
			break;
		case ZWRITE_VAL:
		case ZWRITE_LOWER:
		case ZWRITE_PATTERN:
			arg1 = va_arg(var, int4);
			count--;
			lvzwr_arg(flag, (mval *)arg1, (mval *)0);
			break;
		default:
			GTMASSERT;
			break;
		}
	}
	va_end(var);
	MAXSTR_BUFF_FINI;
}
