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
#include "locklits.h"
#include "mlkdef.h"
#include "zshow.h"

void zshow_format_lock(zshow_out *output, mlk_pvtblk *temp)
{
	static readonly char lp[] = "(";
	static readonly char rp[] = ")";
	static readonly char cm[] = ",";

	mval v;
	unsigned short subs;
	unsigned char *ptr,len;

	ptr = temp->value;

	len = v.str.len = *ptr++;
	v.str.addr = (char *)ptr;
	zshow_output(output, &v.str);
	if (temp->subscript_cnt > 1)
	{
		v.mvtype = MV_STR;
		v.str.len = 1;
		v.str.addr = lp;
		zshow_output(output, &v.str);
		for (subs = 1 ; subs < temp->subscript_cnt; subs++)
		{
			if (subs > 1)
			{
				v.str.len = 1;
				v.str.addr = cm;
				zshow_output(output, &v.str);
			}
			ptr += len;
			len = v.str.len = *ptr++;
			v.str.addr = (char *)ptr;
			mval_write(output, &v, FALSE);
		}
		v.str.len = 1;
		v.str.addr = rp;
		zshow_output(output,&v.str);
	}
}
