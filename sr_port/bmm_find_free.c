/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2024-2026 YottaDB LLC and/or its subsidiaries.	*
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
#include "gdsblk.h"
#include "gdsbml.h"
#include "min_max.h"
#include "gtm_ffs.h"
#include "bmm_find_free.h"

#define MAX_FFS_SIZE 32

#ifdef DEBUG
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"

GBLREF	block_id	ydb_skip_bml_num;
#endif

/* Returns the location of the first set bit in the field.	*/
/* The search starts at the hint and wraps if necessary.	*/

block_id bmm_find_free(block_id hint, uchar_ptr_t base_addr, block_id total_bits)
{
	block_id	start, top, width, answer;

	if (hint >= total_bits)
		hint = 0;
	for (start = hint, top = total_bits;  top;  start = 0, top = hint, hint = 0)
	{	/* one or two passes through outer loop; second is a wrap to the beginning */
#		ifdef DEBUG
		if (0 != ydb_skip_bml_num)
		{	/* Do not waste time trying to find a free bit in bitmaps that lie in the db hole. Bitmap 0 covers
			 * the blocks ahead of the hole and is usable, so a scan that begins there examines it and only
			 * then jumps to the far side.
			 *
			 * "start" is 0 whenever "bm_getfree" was given a hint below BLKS_PER_LMAP, since it passes us
			 * "hint / BLKS_PER_LMAP". That is the case for every allocation while a MUPIP REORG -TRUNCATE is
			 * active in the region, because "bm_getfree" overrides the hint to 1 then, and it is also the case
			 * on the wrap pass of the loop below. The earlier "1 <= start" form skipped nothing in either
			 * case, leaving the entire hole to be scanned a MAX_FFS_SIZE bit window at a time.
			 */
			if (start < (ydb_skip_bml_num / BLKS_PER_LMAP))
			{
				if (0 == start)
				{	/* bitmap 0 is ahead of the hole, so check it before jumping past */
					answer = gtm_ffs(0, base_addr, 1);
					if (NO_FREE_SPACE != answer)
						return answer;
				}
				start = (ydb_skip_bml_num / BLKS_PER_LMAP);
			}
		}
#		endif
		for (width = MIN(top, ROUND_DOWN2(start, MAX_FFS_SIZE) + MAX_FFS_SIZE) - start;  width > 0;
			start += width, width = MIN(top - start, MAX_FFS_SIZE))
		{
			answer = gtm_ffs(start, base_addr, width);
			if (NO_FREE_SPACE != answer)
				return answer;
		}
	}
	assert(NO_FREE_SPACE == answer);
	return NO_FREE_SPACE;
}
