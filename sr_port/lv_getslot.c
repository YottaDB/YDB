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

#include "gtm_stdio.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "caller_id.h"
#include "alias.h"

lv_val *lv_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lv_val		*lv;
	unsigned int	numElems, numUsed;

	numElems = MAXUINT4;	/* maximum value */
	if (lv = sym->lv_flist)
	{
		assert(NULL == LV_PARENT(lv));	/* stp_gcol relies on this for correct garbage collection */
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
			assert(numElems >= p->numAlloc);
			DEBUG_ONLY(numElems = p->numAlloc);
		}
	}
	assert(lv);
	DBGRFCT((stderr, ">> lv_getslot(): Allocating new lv_val at 0x"lvaddr" by routine 0x"lvaddr"\n", lv, caller_id()));
	return lv;
}

lvTree *lvtree_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lvTree		*lvt;
	unsigned int	numElems, numUsed;

	numElems = MAXUINT4;	/* maximum value */
	if (lvt = sym->lvtree_flist)
	{
		assert(NULL == LVT_GET_PARENT(lvt));
		sym->lvtree_flist = (lvTree *)lvt->avl_root;
	} else
	{
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
	DBGRFCT((stderr, ">> lvtree_getslot(): Allocating new lvTree at 0x"lvaddr" by routine 0x"lvaddr"\n", lvt, caller_id()));
	return lvt;
}

lvTreeNode *lvtreenode_getslot(symval *sym)
{
	lv_blk		*p,*q;
	lvTreeNode	*lv;
	unsigned int	numElems, numUsed;

	numElems = MAXUINT4;	/* maximum value */
	if (lv = sym->lvtreenode_flist)
	{
		assert(NULL == LV_PARENT(lv));	/* stp_gcol relies on this for correct garbage collection */
		sym->lvtreenode_flist = (lvTreeNode *)lv->sbs_child;
	} else
	{
		for (p = sym->lvtreenode_first_block; ; p = p->next)
		{
			if (NULL == p)
			{
				if (NULL != (p = sym->lvtreenode_first_block))
					numElems = p->numAlloc;
				else
					numElems = LV_NEWBLOCK_INIT_ALLOC;
				lvtreenode_newblock(sym, numElems > 64 ? 128 : numElems * 2);
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
			lv, caller_id()));
	return lv;
}
