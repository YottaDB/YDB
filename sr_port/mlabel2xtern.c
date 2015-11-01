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
#include "compiler.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "mlabel2xtern.h"

GBLREF command_qualifier cmd_qlf;

void mlabel2xtern(mstr *dst, mident *rtn, mident *lab)
{
	char	*pt, *name, ch;
	int	cnt;

	/* max length is: routine name (mident) . label name (mident) <NUL> + space for case shift characters ($) in VMS labels */
	dst->addr = mcalloc(sizeof(mident) * 3 + sizeof("."));
	pt = dst->addr;
	for (cnt = 0, name = (char *)rtn; cnt < sizeof(mident) && (ch = *name++); cnt++)
		*pt++ = ch;
	*pt++ = '.';
	cnt = 1;
	name = (char *)lab;
	ch = *name++;
	if ('%' == ch)
		ch = '_';
	if (cmd_qlf.qlf & CQ_LOWER_LABELS)
	{
#ifdef VMS
		bool	is_lower, now_lower = FALSE;
#endif
		while (ch)
		{
#ifdef VMS
			/* Note: This toggle is complicated by the fact that we have lower, upper and numerics to deal with */
			is_lower = (ch >= 'a' && ch <= 'z');
			if ((is_lower && !now_lower) || (now_lower && (ch >= 'A' && ch <= 'Z')))
			{
				*pt++ = '$';
				now_lower = !now_lower;
			}
			*pt++ = is_lower ? (ch - ('a' - 'A')) : ch;
#else
			*pt++ = ch;
#endif
			if (cnt++ >= sizeof(mident))
				break;
			ch = *name++;
		}
	} else
	{
		while (ch)
		{
			if (ch >= 'a' && ch <= 'z')
				*pt++ = (ch - ('a' - 'A'));
			else
				*pt++ = ch;
			if (cnt++ >= sizeof(mident))
				break;
			ch = *name++;
		}
	}
	dst->len = pt - dst->addr;
	return;
}
