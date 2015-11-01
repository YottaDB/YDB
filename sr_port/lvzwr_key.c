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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "gtm_string.h"

GBLREF lvzwrite_struct lvzwrite_block;

unsigned char *lvzwr_key(unsigned char *buff, int size)
{
	int	sub_idx;
	mstr	sub;
	unsigned char *cp, *cq;

	for (cp = (unsigned char *)lvzwrite_block.curr_name, cq = cp + sizeof(mident); cp < cq && *cp && size; cp++)
	{
		*buff++ = *cp;
		size--;
	}
	if (lvzwrite_block.subsc_count)
	{
		if (size)
		{
			*buff++ = '(';
			size--;
		}
		for (sub_idx = 0; ; )
		{
			mval_lex(((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[sub_idx].actual, &sub);
			if (0 <= (size -= sub.len))
			{
				memcpy(buff, sub.addr, sub.len);
				buff += sub.len;
			} else
				break;
			if (++sub_idx < lvzwrite_block.curr_subsc && size)
			{
				*buff++ = ',';
				size--;
			} else
			{
				if (size)
				{
					*buff++ = ')';
					size--;
				}
				break;
			}
		}
	}
	return buff;
}
