/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_ffs.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "gdsfhead.h"

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

#define BITS_PER_UCHAR	8

block_id gtm_ffs (block_id offset, uchar_ptr_t addr, block_id size)
{
	uchar_ptr_t	c;
	block_id	i, j, top;

	c = addr + (offset / BITS_PER_UCHAR);
	if ((i = (offset & (BITS_PER_UCHAR - 1))))
	{	/* partial byte starting at offset */
		for (j = 0;  (i < BITS_PER_UCHAR) && (j < size);  j++, i++)
		{
#			ifdef DEBUG
			if ((0 != ydb_skip_bml_num) && (1 <= (offset + j)) && ((offset + j) < (ydb_skip_bml_num / BLKS_PER_LMAP)))
				continue;
#			endif
			if (*c & (1 << i))
				return (offset + j);
		}
		c++;
	}
	assert(c == (addr + (offset + BITS_PER_UCHAR - 1) / BITS_PER_UCHAR));
	for (i = ROUND_UP2(offset, BITS_PER_UCHAR), top = ROUND_DOWN2(size + offset, BITS_PER_UCHAR);  i < top;
		c++, i += BITS_PER_UCHAR)
	{	/* full bytes offset to end */
		if (*c)
		{
			for (j = 0;  j < BITS_PER_UCHAR;  j++)
			{
#				ifdef DEBUG
				if ((0 != ydb_skip_bml_num) && (1 <= (i + j)) && ((i + j) < (ydb_skip_bml_num / BLKS_PER_LMAP)))
					continue;
#				endif
				if (*c & (1 << j))
					return (i + j);
			}
		}
	}
	for (j = 0, top = size + offset;  i < top;  j++, i++)
	{	/* partial byte at end */
		assert(j < BITS_PER_UCHAR);
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (1 <= i) && (i < (ydb_skip_bml_num / BLKS_PER_LMAP)))
			continue;
#		endif
		if (*c & (1 << j))
			return i;
	}
	return -1;
}
