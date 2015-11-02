/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF lv_val		*active_lv;
GBLREF uint4		dollar_tlevel;

void	lv_kill(lv_val *lv, boolean_t dotpsave, boolean_t do_subtree)
{
	lv_val		*base_lv;
	lvTree		*lvt_child, *lvt;
	boolean_t	is_base_var;
	symval		*sym;

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
	if (lv)
	{
		is_base_var = LV_IS_BASE_VAR(lv);
		base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
		if (dotpsave && dollar_tlevel && (NULL != base_lv->tp_var) && !base_lv->tp_var->var_cloned)
			TP_VAR_CLONE(base_lv);	/* clone the tree */
		lvt_child = LV_GET_CHILD(lv);
		if (do_subtree && (NULL != lvt_child))
		{
			LV_CHILD(lv) = NULL;
		      	lv_killarray(lvt_child, dotpsave);
		}
		DECR_AC_REF(lv, dotpsave);		/* Decrement alias container refs and cleanup if necessary */
		if (!is_base_var && (do_subtree || (NULL == lvt_child)))
		{
			sym = LV_GET_SYMVAL(base_lv);
			for ( ; ; )
			{
				lvt = LV_GET_PARENT_TREE(lv);
				LV_VAL_CLEAR_MVTYPE(lv); /* see comment in macro definition for why this is necessary */
				LV_TREE_NODE_DELETE(lvt, (lvTreeNode *)lv);
				/* if there is at least one other sibling node to the deleted "lv" the zap stops here */
				if (lvt->avl_height)
					break;
				assert(NULL == lvt->avl_root);
				lv = (lv_val *)LVT_PARENT(lvt);
				assert(NULL != lv);
				LV_CHILD(lv) = NULL;
				LVTREE_FREESLOT(lvt);
				assert(LV_IS_VAL_DEFINED(lv) == (0 != lv->v.mvtype));
				if (LV_IS_VAL_DEFINED(lv))
					break;
				if (lv == base_lv)
				{	/* Base node. Do not invoke LV_FREESLOT/LV_FLIST_ENQUEUE as we will still keep the
					 * lv_val for the non-existing base local variable pointed to by the curr_symval
					 * hash table entry. Just clear mvtype to mark the lv_val undefined.
					 */
					lv->v.mvtype = 0;	/* Base node */
					break;
				}
			}
		} else
			lv->v.mvtype = 0;	/* Base node */
	}
}
