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
#include "stack_frame.h"
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "view.h"

#define S_CUTOFF 7
GBLREF rtn_tables *rtn_names,*rtn_names_end;
GBLREF stack_frame *frame_pointer;

void view_routines(mval *dst,mident *name)
{
	unsigned short  len;
	mident		temp;
	rtn_tables	*bot, *top, *mid;
	int4		comp;

	len = mid_len(name);

#ifdef UNIX
	memcpy(&temp.c[0], name, sizeof(mident));
#else
	lower_to_upper(&temp.c[0], name, sizeof(mident));
#endif

	bot = rtn_names;
	top = rtn_names_end;
	while (bot->rt_name.c[0] < '%' && bot < top)
		bot++;

	assert(bot <= top);
	if (!len)
	{	dst->str.addr = &bot->rt_name.c[0];
		dst->str.len = mid_len(&bot->rt_name);
		s2pool(&dst->str);
		return;
	}

	for (;;)
	{	if ((char *) top - (char *) bot < S_CUTOFF * sizeof(rtn_tables))
		{
			comp = -1;
			for (mid = bot; comp < 0 && mid <= top ;mid++)
			{
				comp = memcmp(mid->rt_name.c, &temp, sizeof(mident));
				if (!comp)
				{
					if (mid != rtn_names_end)
					{
						mid++;
						dst->str.addr = &mid->rt_name.c[0];
						dst->str.len = mid_len(&mid->rt_name);
						s2pool(&dst->str);
					}
					else
						dst->str.len = 0;
					return;
				}
				else if (comp < 0)
					continue;
				else
				{	dst->str.addr = &mid->rt_name.c[0];
					dst->str.len = mid_len(&mid->rt_name);
					s2pool(&dst->str);
					return;
				}
			}
			dst->str.len = 0;
			return;
		}
		else
		{
			mid = bot + (top - bot)/2;
			comp = memcmp(mid->rt_name.c, &temp, sizeof(mident));
			if (!comp)
			{	if (mid != rtn_names_end)
				{
					mid++;
					dst->str.addr = &mid->rt_name.c[0];
					dst->str.len = mid_len(&mid->rt_name);
					s2pool(&dst->str);
				}
				else
					dst->str.len = 0;
				return;
			}
			else if (comp < 0)
			{	bot = mid + 1;
				continue;
			}
			else
			{	top = mid;
				continue;
			}
		}
	}
}
