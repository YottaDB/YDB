/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "min_max.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "gtm_string.h"

GBLREF lvzwrite_datablk *lvzwrite_block;

unsigned char *lvzwr_key(unsigned char *buff, int size)
{
	int	sub_idx, len;
	mstr	sub;
	unsigned char *cp, *cq;

	assert(lvzwrite_block);
	len = MIN(size, lvzwrite_block->curr_name->len);
	assert(MAX_MIDENT_LEN >= len);
	memcpy((void *)buff, lvzwrite_block->curr_name->addr, len);
	size -= len;
	buff += len;

	if (lvzwrite_block->subsc_count)
	{
		if (size)
		{
			*buff++ = '(';
			size--;
		}
		for (sub_idx = 0; ; )
		{
			mval_lex(((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[sub_idx].actual, &sub);
			if (0 <= (size -= sub.len))
			{
				memcpy((void *)buff, sub.addr, sub.len);
				buff += sub.len;
			} else
				break;
			if (++sub_idx < lvzwrite_block->curr_subsc && size)
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
