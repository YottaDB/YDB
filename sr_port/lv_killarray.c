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

STATICDEF	boolean_t	dotpsave_static;

static void	lv_killarray_base(tree *lvt);
static void	lv_killarray_recurse(treeNode *node);

void lv_killarray(tree *lvt, boolean_t dotpsave)
{
	dotpsave_static = dotpsave;	/* save dotpsave in a static to avoid having to pass it all through the recursion */
	lv_killarray_base(lvt);
}

/* Note it is important that callers of this routine make sure that the pointer that is passed as
 * an argument is removed from the lv_val it came from prior to the call. This prevents arrays
 * that have alias containers pointing that form a loop back to the originating lv_val from causing
 * processing loops, in effect over-processing arrays that have already been processed or lv_vals
 * that have been deleted.
 *
 * Note the below could have been implemented using iteration (using lvTreeNodeFirst and lvTreeNodeNext in a loop to
 * scan all nodes in the tree) OR using recursion. We choose recursion because in this case, we are killing/freeing nodes
 * as we scan them and to avoid risk of issues with accessing a freed pointer, we choose post-order traversal (where you
 * process/kill/free a node only AFTER processing both its left and right subtrees) which is best done using recursion.
 * If not for that, we would have preferred iteration as that is less intensive on the C stack.
 */
static void	lv_killarray_base(tree *lvt)
{
	DEBUG_ONLY(
		lv_val		*lv;

		lv = (lv_val *)lvt->sbs_parent;
		assert(NULL == LV_CHILD(lv));	/* Owner lv's children pointer MUST be NULL! */
	)
	lvTreeWalkPostOrder(lvt, lv_killarray_recurse);
	LVTREE_FREESLOT(lvt);
	return;
}

static void	lv_killarray_recurse(treeNode *node)
{
	tree	*lvt;

	assert(NULL != node);
	lvt = LV_CHILD(node);
	if (NULL != lvt)
	{
		LV_CHILD(node) = NULL;
		lv_killarray_base(lvt);
	}
	DECR_AC_REF(((lv_val *)node), dotpsave_static);	/* Decrement alias contain ref and cleanup if necessary */
	/* If node points to an "lv_val", we need to do a heavyweight LV_FREESLOT call to free up the lv_val.
	 * But we instead do a simple "LVTREENODE_FREESLOT" call because we are guaranteed node points to a "treeNode"
	 * (i.e. it is a subscripted lv and never the base lv). Assert that.
	 */
	assert(!LV_IS_BASE_VAR(node));
	LV_VAL_CLEAR_MVTYPE(node);      /* see comment in macro definition for why this is necessary */
	LVTREENODE_FREESLOT(node);
}
