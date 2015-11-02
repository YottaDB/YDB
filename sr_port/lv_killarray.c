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
#include "gtm_string.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

/* Note it is important that callers of this routine make sure that the pointer that is passed as
 * an argument is removed from the lv_val it came from prior to the call. This prevents arrays
 * that have alias containers pointing that form a loop back to the originating lv_val from causing
 * processing loops, in effect over-processing arrays that have already been processed or lv_vals
 * that have been deleted.
 */
void lv_killarray(lvTree *lvt, boolean_t dotpsave)
{
	lvTreeNode	*node, *nextnode;
	lvTree		*tmplvt;

	DEBUG_ONLY(
		lv_val		*lv;

		assert(NULL != lvt);
		lv = (lv_val *)LVT_PARENT(lvt);
		assert(NULL == LV_CHILD(lv));	/* Owner lv's children pointer MUST be NULL! */
	)
	/* Iterate through the tree in post-order fashion. Doing it in-order or pre-order has issues since we would have
	 * freed up nodes in the tree but would need to access links in them to get at the NEXT node.
	 */
	for (node = lvAvlTreeFirstPostOrder(lvt); NULL != node; node = nextnode)
	{
		nextnode = lvAvlTreeNextPostOrder(node);	/* determine "nextnode" before freeing "node" */
		assert(NULL != node);
		tmplvt = LV_CHILD(node);
		if (NULL != tmplvt)
		{
			LV_CHILD(node) = NULL;
			lv_killarray(tmplvt, dotpsave);
		}
		DECR_AC_REF(((lv_val *)node), dotpsave);	/* Decrement alias contain ref and cleanup if necessary */
		/* If node points to an "lv_val", we need to do a heavyweight LV_FREESLOT call to free up the lv_val.
		 * But we instead do a simple "LVTREENODE_FREESLOT" call because we are guaranteed node points to a "lvTreeNode"
		 * (i.e. it is a subscripted lv and never the base lv). Assert that.
		 */
		assert(!LV_IS_BASE_VAR(node));
		LV_VAL_CLEAR_MVTYPE(node);      /* see comment in macro definition for why this is necessary */
		LVTREENODE_FREESLOT(node);
	}
	LVTREE_FREESLOT(lvt);
}
