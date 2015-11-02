/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"

lv_blk *lv_newblock(lv_blk *block_addr, lv_blk *next_block, int size)
{
	register lv_blk *ptr;
	register int n;

	ptr = block_addr;
	n = size * SIZEOF(lv_val);
	ptr->lv_base = ptr->lv_free = (lv_val *)malloc(n);
	memset(ptr->lv_base, 0, n);
	ptr->lv_top = ptr->lv_base + size;
	ptr->next = next_block;
	return ptr;
}
