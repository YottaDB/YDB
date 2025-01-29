/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
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
#include "gtm_maxstr.h"
#include "alias.h"
#include "lv_val.h"

GBLREF  uint4   	zwrtacindx;
GBLREF  int     	merge_args;
GBLREF  symval  	*curr_symval;
GBLREF  zwr_hash_table  *zwrhtab;

LITREF  mstr    	dzwrtac_clean;

void op_lvzwrite(UNIX_ONLY_COMMA(int4 count) long arg1, ...)
{
	va_list		var;
	boolean_t	flag;
	long		arg2;
	VMS_ONLY(int4	count;)
	mval		*mv;
	zshow_out	output, *out;
	DCL_THREADGBL_ACCESS;
	MAXSTR_BUFF_DECL(buff);

	SETUP_THREADGBL_ACCESS;
	VMS_ONLY(va_count(count));
	MAXSTR_BUFF_INIT;

	merge_args = 0;
	memset(&output, 0, SIZEOF(output));
	output.code = 'V';
	output.stack_level = STACK_LEVEL_MINUS_ONE;
	output.type = ZSHOW_DEVICE;
	output.buff = &buff[0];
	output.size = SIZEOF(buff);
	output.ptr = output.buff;
	out = &output;
	count--;
	VAR_START(var, arg1);
	lvzwr_init(zwr_patrn_mident, (mval *)arg1);
	for (; count > 0; )
	{
		mv = va_arg(var, mval *); count--;
		switch ((flag = MV_FORCE_INT(mv)))
		{
			case ZWRITE_ASTERISK:
				lvzwr_arg(flag, (mval *)0, (mval *)0); /* caution fall through */
			case ZWRITE_END:
				lvzwr_fini(out, flag);
				if (zwrtacindx)
				{       /* If we output some $ZWRTAC stuff, send one last line to close it up */
					assert (!merge_args && curr_symval->alias_activity);
					out->flush = TRUE;
					zshow_output(out, &dzwrtac_clean);
					zwrtacindx = 0;
				}
				if (zwrhtab && !zwrhtab->cleaned)
					als_zwrhtab_init();
				va_end(var);
				MAXSTR_BUFF_FINI;
				TREF(in_zwrite) = FALSE;
				return;
			case ZWRITE_ALL:
				lvzwr_arg(flag, (mval *)0, (mval *)0);
				break;
			case ZWRITE_BOTH:
				arg1 = va_arg(var, long);
				arg2 = va_arg(var, long);
				count -= 2;
				lvzwr_arg(flag, (mval *)arg1, (mval *)arg2);
				break;
			case ZWRITE_UPPER:
				arg1 = va_arg(var, long);
				count--;
				lvzwr_arg(flag, (mval *)0, (mval *)arg1);
				break;
			case ZWRITE_VAL:
			case ZWRITE_LOWER:
			case ZWRITE_PATTERN:
				arg1 = va_arg(var, long);
				count--;
				lvzwr_arg(flag, (mval *)arg1, (mval *)0);
				break;
			default:
				assertpro(FALSE);
				break;
		}
	}
	va_end(var);
	MAXSTR_BUFF_FINI;
	TREF(in_zwrite) = FALSE;
}
