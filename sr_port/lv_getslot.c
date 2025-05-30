/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*

 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "gtmio.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "caller_id.h"
#include "alias.h"
#include "stack_frame.h"

GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;

/* Allocate a local variable slot (lv_val) in an lv_blk that contains one or more unallocated entries.
 *
 * Argument: symval pointer to allocate the lv_val from.
 * Returns:  allocated lv_val pointer.
 */
lv_val *lv_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lv_val		*lv;
	unsigned int	numElems, numUsed;

	if ((lv = sym->lv_flist))
	{
		assert(NULL == LV_PARENT(lv));		/* stp_gcol relies on this for correct garbage collection */
		sym->lv_flist = (lv_val *)lv->ptrs.free_ent.next_free;
	} else
	{
		for (p = sym->lv_first_block; ; p = p->next)
		{
			if (NULL == p)
			{
				if (NULL != (p = sym->lv_first_block))
					numElems = p->numAlloc;
				else
				{
					assert(FALSE);
					numElems = LV_NEWBLOCK_INIT_ALLOC;	/* be safe in pro */
				}
				lv_newblock(sym, numElems > 64 ? 128 : numElems * 2);
				p = sym->lv_first_block;
				assert(NULL != p);
			}
			if ((numUsed = p->numUsed) < p->numAlloc)
			{
				lv = (lv_val *)LV_BLK_GET_BASE(p);
				lv = &lv[numUsed];
				p->numUsed++;
				break;
			}
			/* Note: The following assert exists in "lvtree_getslot()" and "lvtreenode_getslot()".
			 *	assert(numElems >= p->numAlloc);
			 * This assert basically says that the "sym->lv_first_block" linked list has the property
			 * that earlier nodes in the linked list have a "numAlloc" that is greater or equal to
			 * later nodes. This is guaranteed to be true since the only allocations for lvtree and
			 * lvtreenode happen inside "lvtree_getslot()" and "lvtreenode_getslot()" respectively
			 * where the allocation is incrementally done and doubled at every node until it is maxed
			 * out at 128.
			 *
			 * One would be tempted to have a similar assert here. But it is not guaranteed
			 * to be true since "lv_newblock()" is called not just in "lv_getslot()" but also in
			 * "sr_port/symbinit.c" with a bulk allocation based on the number of local variables used
			 * in a given M program. This means that the initial allocation could be greater than 128
			 * (see YDB#1088 issue description for example). In that case, later allocations would get
			 * maxed out at "128" and so the assert would fail. Hence the assert is commented out here.
			 */
		}
	}
	assert(lv);
	DBGRFCT((stderr, "\n>> lv_getslot(): Allocating new lv_val at 0x"lvaddr" by routine 0x"lvaddr" at mpc 0x"lvaddr
		 " for symval 0x"lvaddr" (curr_symval: 0x"lvaddr")\n", lv, caller_id(0), frame_pointer->mpc, sym, curr_symval));
	return lv;
}

lvTree *lvtree_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lvTree		*lvt;
	unsigned int	numElems, numUsed;

	if ((lvt = sym->lvtree_flist))
	{
		assert(NULL == LVT_GET_PARENT(lvt));
		sym->lvtree_flist = (lvTree *)lvt->avl_root;
	} else
	{
		DEBUG_ONLY(numElems = MAXUINT4);	/* maximum value */
		for (p = sym->lvtree_first_block; ; p = p->next)
		{
			if (NULL == p)
			{
				if (NULL != (p = sym->lvtree_first_block))
					numElems = p->numAlloc;
				else
					numElems = LV_NEWBLOCK_INIT_ALLOC;
				lvtree_newblock(sym, numElems > 64 ? 128 : numElems * 2);
				p = sym->lvtree_first_block;
				assert(NULL != p);
			}
			if ((numUsed = p->numUsed) < p->numAlloc)
			{
				lvt = (lvTree *)LV_BLK_GET_BASE(p);
				lvt = &lvt[numUsed];
				p->numUsed++;
				break;
			}
			assert(numElems >= p->numAlloc);
			DEBUG_ONLY(numElems = p->numAlloc);
		}
	}
	assert(lvt);
	DBGRFCT((stderr, ">> lvtree_getslot(): Allocating new lvTree at 0x"lvaddr" by routine 0x"lvaddr"\n", lvt, caller_id(0)));
	return lvt;
}

lvTreeNode *lvtreenode_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lvTreeNode	*lv;
	unsigned int	numElems, numUsed;

	if ((lv = sym->lvtreenode_flist))
	{
		assert(NULL == LV_PARENT(lv));	/* stp_gcol relies on this for correct garbage collection */
		sym->lvtreenode_flist = (lvTreeNode *)lv->sbs_child;
	} else
	{
		DEBUG_ONLY(numElems = MAXUINT4);	/* maximum value */
		for (p = sym->lvtreenode_first_block; ; p = p->next)
		{
			if (NULL == p)
			{
				if (NULL != (p = sym->lvtreenode_first_block))
					numElems = p->numAlloc;
				else
					numElems = LV_NEWBLOCK_INIT_ALLOC;
				lvtreenode_newblock(sym, (numElems <= (MAXINT4 / 2)) ? (numElems * 2) : MAXINT4);
				p = sym->lvtreenode_first_block;
				assert(NULL != p);
			}
			if ((numUsed = p->numUsed) < p->numAlloc)
			{
				lv = (lvTreeNode *)LV_BLK_GET_BASE(p);
				lv = &lv[numUsed];
				p->numUsed++;
				break;
			}
			assert(numElems >= p->numAlloc);
			DEBUG_ONLY(numElems = p->numAlloc);
		}
	}
	assert(lv);
	DBGRFCT((stderr, ">> lvtreenode_getslot(): Allocating new lvTreeNode at 0x"lvaddr" by routine 0x"lvaddr"\n",
			lv, caller_id(0)));
	return lv;
}
