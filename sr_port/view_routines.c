/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "stack_frame.h"
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "view.h"
#include "min_max.h"	/* MIDENT_CMP needs MIN */

#define S_CUTOFF 7

GBLREF rtn_tabent	*rtn_names, *rtn_names_end;
GBLREF stack_frame 	*frame_pointer;

void view_routines(mval *dst, mident_fixed *name)
{
	mident		temp;
	char		temp_buff[SIZEOF(mident_fixed)];
	rtn_tabent	*bot, *top, *mid;
	int4		comp;

	temp.len = INTCAST(mid_len(name));
#ifdef UNIX
	temp.addr = &name->c[0];
#else
	lower_to_upper(&temp_buff[0], &name->c[0], temp.len);
	temp.addr = &temp_buff[0];
#endif

	bot = rtn_names;
	top = rtn_names_end;
	/* Skip over all routines that are not created by the user. These routines include
	 * the dummy null routine, $FGNXEC (created for DALs) and $FGNFNC (created for Call-ins)
	 * which are guaranteed to sort before any valid M routine */
	while (bot < top && (0 == bot->rt_name.len || '%' > bot->rt_name.addr[0]))
		bot++;
	assert(bot <= top);
	if (!temp.len)
	{	dst->str.addr = bot->rt_name.addr;
		dst->str.len = bot->rt_name.len;
		s2pool(&dst->str);
		return;
	}

	for (;;)
	{
		if ((top - bot) < S_CUTOFF)
		{
			comp = -1;
			for (mid = bot; comp < 0 && mid <= top ;mid++)
			{
				MIDENT_CMP(&mid->rt_name, &temp, comp);
				if (!comp)
				{
					if (mid != rtn_names_end)
					{
						mid++;
						dst->str.addr = mid->rt_name.addr;
						dst->str.len = mid->rt_name.len;
						s2pool(&dst->str);
					}
					else
						dst->str.len = 0;
					return;
				}
				else if (comp < 0)
					continue;
				else
				{	dst->str.addr = mid->rt_name.addr;
					dst->str.len = mid->rt_name.len;
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
			MIDENT_CMP(&mid->rt_name, &temp, comp);
			if (!comp)
			{	if (mid != rtn_names_end)
				{
					mid++;
					dst->str.addr = mid->rt_name.addr;
					dst->str.len = mid->rt_name.len;
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
