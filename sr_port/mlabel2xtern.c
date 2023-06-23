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

#include "gtm_ctype.h"
#include "gtm_string.h"

#include "cmd_qlf.h"
#include "mmemory.h"
#include "mlabel2xtern.h"

GBLREF command_qualifier cmd_qlf;

void mlabel2xtern(mstr *dst, mident *rtn, mident *lab)
{
	char	*pt;
	int	cnt;

	/* length of the resultant symbol "<routine name>.<label name>" */
	pt = dst->addr = mcalloc(rtn->len + lab->len +(unsigned int)(STR_LIT_LEN(".")));
	memcpy(pt, rtn->addr, rtn->len);
	pt += rtn->len;
	*pt++ = '.';

	if (0 < lab->len)
	{
		if (cmd_qlf.qlf & CQ_LOWER_LABELS)
		{
			memcpy(pt, lab->addr, lab->len);
			if ('%' == *pt)
				*pt = '_';
			pt += lab->len;
		} else
		{
			cnt = 0;
			if ('%' == lab->addr[cnt])
			{
				*pt++ = '_';
				cnt++;
			}
			for (; cnt < lab->len; cnt++)
				*pt++ = TOUPPER(lab->addr[cnt]);
		}
	}
	dst->len = INTCAST(pt - dst->addr);
	return;
}
