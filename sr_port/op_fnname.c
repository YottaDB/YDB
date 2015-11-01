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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "fnname.h"
#include <varargs.h>
#include "op.h"
#include "gvsub2str.h"

GBLREF gv_key		*gv_currkey;
GBLREF spdesc		stringpool;

void op_fnname(va_alist)
va_dcl
{
	int		space_needed, i;
	int 		sub_count, depth_count, fnname_type;
	mval		*dst, *v;
	mstr		format_out;
	va_list		var, argbase;
	unsigned char	*sptr, *key_ptr, *key_top;
	error_def(ERR_MAXSTRLEN);
	error_def(ERR_GVNAKED);
	error_def(ERR_FNNAMENEG);

	VAR_START(var);
	sub_count = va_arg(var, int);
	dst = va_arg(var, mval *);
	fnname_type = va_arg(var, int);
	depth_count = va_arg(var, int);
	sub_count -=3;

	if (depth_count < 0)
		rts_error(VARLSTCNT(1) ERR_FNNAMENEG);

	VAR_COPY(argbase, var);
	/* determine if garbage collection is required */
	space_needed = 3;	/* space for ^[] */
	for (i = 0; i < sub_count ; i++)
	{
		v = va_arg(var, mval *);
		mval_lex(v, &format_out);
		space_needed += format_out.len + 1;
	}
	if (fnname_type == FNNAKGBL)
		if (gv_currkey)
			space_needed += gv_currkey->top;

	if (space_needed > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);

	if (stringpool.free + space_needed > stringpool.top)
		stp_gcol(space_needed);		/* get a big buffer */
	sptr = stringpool.free;

	VAR_COPY(var, argbase);

	if (fnname_type == FNNAKGBL)
	{
		if (!gv_currkey || gv_currkey->prev == 0)
			rts_error(VARLSTCNT(1) ERR_GVNAKED);
		*stringpool.free++ = '^';
		key_ptr = (unsigned char *)&gv_currkey->base[0];
		key_top = (unsigned char *)&gv_currkey->base[ gv_currkey->prev ];
		for ( ; (*stringpool.free = *key_ptr++); stringpool.free++)
		{	;
		}
		if (depth_count)
		{	*stringpool.free++ = '(';
			if (key_ptr < key_top)
				for ( ; ; )
				{	stringpool.free = gvsub2str(key_ptr, stringpool.free, TRUE);
					while(*key_ptr++)
						;
					depth_count--;
					*stringpool.free++ = ',';
					if (!depth_count || key_ptr >= key_top)
						break;
				}
			depth_count = sub_count < depth_count ? sub_count : depth_count;
			if (depth_count)
			{
				for ( ; ; )
				{
					v = va_arg(var, mval *);
					mval_lex(v, &format_out);
					if (format_out.addr != (char *)stringpool.free)
						memcpy(stringpool.free, format_out.addr, format_out.len);
					stringpool.free += format_out.len;
					depth_count--;
					if (!depth_count)
						break;
					*stringpool.free++ = ',';
				}
			}
			else
				stringpool.free--;	/* take off extra , if depth doesn't go into new subs */
			*stringpool.free++ = ')';
		}
	}
	else
	{
		if (fnname_type & FNGBL)
		{	*stringpool.free++ = '^';
		}
		if (fnname_type & (FNEXTGBL1 | FNEXTGBL2))
		{	*stringpool.free++ = fnname_type & FNVBAR ? '|' : '[';
			v = va_arg(var, mval *);
			mval_lex(v, &format_out);
			if (format_out.addr != (char *)stringpool.free)
				memcpy(stringpool.free, format_out.addr, format_out.len);
			stringpool.free += format_out.len;
			sub_count--;
			if (fnname_type & FNEXTGBL2)
			{	*stringpool.free++ = ',';
				v = va_arg(var, mval *);
				mval_lex(v, &format_out);
				if (format_out.addr != (char *)stringpool.free)
					memcpy(stringpool.free, format_out.addr, format_out.len);
				stringpool.free += format_out.len;
				sub_count--;
			}
			*stringpool.free++ = fnname_type & FNVBAR ? '|' : ']';
		}
		v = va_arg(var, mval *);
		assert(MV_IS_STRING(v) && v->str.len <= sizeof(mident));
		memcpy(stringpool.free, v->str.addr, v->str.len);
		stringpool.free += v->str.len;
		sub_count--;
		depth_count = sub_count < depth_count ? sub_count : depth_count;
		if (depth_count)
		{	*stringpool.free++ = '(';
			for ( ; ; )
			{	v = va_arg(var, mval *);
				mval_lex(v, &format_out);
				if (format_out.addr != (char *)stringpool.free)
					memcpy(stringpool.free, format_out.addr, format_out.len);
				stringpool.free += format_out.len;
				depth_count--;
				if (!depth_count)
					break;
				*stringpool.free++ = ',';
			}
			*stringpool.free++ = ')';
		}
	}
	dst->mvtype = MV_STR;
	dst->str.len = stringpool.free - sptr;
	dst->str.addr = (char *)sptr;
	assert(space_needed >= dst->str.len);
}
