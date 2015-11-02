/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <rtnhdr.h>
#include "ident.h"
#include "min_max.h"

#define S_CUTOFF 7
GBLREF rtn_tabent	*rtn_names, *rtn_names_end;

rhdtyp	*find_rtn_hdr(mstr *name)
{
	rtn_tabent	*bot, *top, *mid;
	int4		comp;
	mident		rtn_name;
	mident_fixed	rtn_name_buff;

	assert (name->len <= MAX_MIDENT_LEN);
	rtn_name.len = name->len;
#ifdef VMS
	rtn_name.addr = &rtn_name_buff.c[0];
	CONVERT_IDENT(rtn_name.addr, name->addr, name->len);
#else
	rtn_name.addr = name->addr;
#endif
	bot = rtn_names;
	top = rtn_names_end;
	for (;;)
	{
		if (top < bot)
			return 0;
		else if ((top - bot) < S_CUTOFF)
		{
			comp = -1;
			for (mid = bot; comp < 0 && mid <= top; mid++)
			{
				MIDENT_CMP(&mid->rt_name, &rtn_name, comp);
				if (0 == comp)
					return mid->rt_adr;
			}
			return 0;
		} else
		{
			mid = bot + (top - bot) / 2;
			MIDENT_CMP(&mid->rt_name, &rtn_name, comp);
			if (0 == comp)
				return mid->rt_adr;
			else if (comp < 0)
			{
				bot = mid + 1;
				continue;
			} else
			{
				top = mid - 1;
				continue;
			}
		}
	}
}
