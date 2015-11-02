/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

void	lv_newblock(symval *sym, int numElems)
{
	register lv_blk	*ptr;
	register int	n;
	lv_val		*lv_base;

	n = numElems * SIZEOF(lv_val) + SIZEOF(lv_blk);
	n = INTCAST(gtm_bestfitsize(n));
	/* Maximize use of storage block we are going to get */
	assert(DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lv_val)) >= numElems);
	numElems = DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lv_val));
	ptr = (lv_blk *)malloc(n);
	lv_base = (lv_val *)LV_BLK_GET_BASE(ptr);
	memset(lv_base, 0, numElems * SIZEOF(lv_val));
	ptr->next = sym->lv_first_block;
	sym->lv_first_block = ptr;
	ptr->numAlloc = numElems;
	ptr->numUsed = 0;
	DBGRFCT((stderr, "lv_newblock: New lv_blk allocated ******************\n"));
}

void	lvtree_newblock(symval *sym, int numElems)
{
	register lv_blk	*ptr;
	register int	n;
	lvTree		*lvt_base;

	n = numElems * SIZEOF(lvTree) + SIZEOF(lv_blk);
	n = INTCAST(gtm_bestfitsize(n));
	/* Maximize use of storage block we are going to get */
	assert(DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lvTree)) >= numElems);
	numElems = DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lvTree));
	ptr = (lv_blk *)malloc(n);
	lvt_base = (lvTree *)LV_BLK_GET_BASE(ptr);
	ptr->next = sym->lvtree_first_block;
	sym->lvtree_first_block = ptr;
	ptr->numAlloc = numElems;
	ptr->numUsed = 0;
	DBGRFCT((stderr, "lvtree_newblock: New lv_blk allocated ******************\n"));
}

void	lvtreenode_newblock(symval *sym, int numElems)
{
	register lv_blk	*ptr;
	register int	n;
	lvTreeNode	*lv_base;

	n = numElems * SIZEOF(lvTreeNode) + SIZEOF(lv_blk);
	n = INTCAST(gtm_bestfitsize(n));
	/* Maximize use of storage block we are going to get */
	assert(DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lvTreeNode)) >= numElems);
	numElems = DIVIDE_ROUND_DOWN(n - SIZEOF(lv_blk), SIZEOF(lvTreeNode));
	ptr = (lv_blk *)malloc(n);
	lv_base = (lvTreeNode *)LV_BLK_GET_BASE(ptr);
	memset(lv_base, 0, numElems * SIZEOF(lvTreeNode));
	ptr->next = sym->lvtreenode_first_block;
	sym->lvtreenode_first_block = ptr;
	ptr->numAlloc = numElems;
	ptr->numUsed = 0;
	DBGRFCT((stderr, "lvtreenode_newblock: New lv_blk allocated ******************\n"));
}
