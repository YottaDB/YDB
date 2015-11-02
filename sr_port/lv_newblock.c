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
#include "gtm_stdio.h"

#include "gtm_malloc.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

lv_blk *lv_newblock(lv_blk *block_addr, lv_blk *next_block, int size)
{
	register lv_blk	*ptr;
	register int	n;

	ptr = block_addr;
	n = size * SIZEOF(lv_val);
	/* Maximize use of storage block we are going to get */
	n = ROUND_DOWN(INTCAST(gtm_bestfitsize(n)), SIZEOF(lv_val));
	ptr->lv_base = ptr->lv_free = (lv_val *)malloc(n);
	memset(ptr->lv_base, 0, n);
	ptr->lv_top = ptr->lv_base + (n / SIZEOF(lv_val));
	ptr->next = next_block;
	DBGRFCT((stderr, "lv_newblock: New lv_blk allocated ******************\n"));
	return ptr;
}
