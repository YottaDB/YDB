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
#include "rtnhdr.h"
#include "ident.h"

#define S_CUTOFF 7
GBLREF rtn_tables	*rtn_names, *rtn_names_end;

rhdtyp	*find_rtn_hdr(name)
mstr	*name;
{
	mident		temp;
	rtn_tables	*bot, *top, *mid;
	int4		comp;

	assert (name->len <= sizeof(mident));

	memset(&temp.c[0], 0, sizeof(mident));
	CONVERT_IDENT(&temp.c[0], name->addr, name->len);
	bot = rtn_names;
	top = rtn_names_end;
	for (;;)
	{
		if (top < bot)
			return 0;
		else if ((char *)top - (char *)bot < S_CUTOFF * sizeof(rtn_tables))
		{
			comp = -1;
			for (mid = bot;  comp < 0  &&  mid <= top;  mid++)
			{
				comp = memcmp(mid->rt_name.c, &temp, sizeof(mident));
				if (comp == 0)
				{
					return (rhdtyp *)mid->rt_ptr;
				}
			}
			return 0;
		}
		else
		{
			mid = bot + (top - bot)/2;
			comp = memcmp(mid->rt_name.c, &temp, sizeof(mident));
			if (comp == 0)
				return (rhdtyp *) mid->rt_ptr;
			else if (comp < 0)
			{
				bot = mid + 1;
				continue;
			}
			else
			{
				top = mid - 1;
				continue;
			}
		}
	}
}
