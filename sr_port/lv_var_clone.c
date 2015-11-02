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

#include <stddef.h>		/* For offsetof() macro */

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

/* Routine to clone the children of a tree. The input lv_val "clone_var" should be a clone of the base lv_val
 * owning the tree we wish to clone. The pointers in this copy will be duplicated and the new tree linked to
 * "clone_var".  Note that the owning symval of clone_var should be set appropriately such as in the case of
 * xnew processing where we are cloneing a tree out of one symtab and into another. The new tree will be
 * created in the symtab of the input lv_val regardless of which symtab owns the processed lv_vals.
 *
 * Input "base_lv" is the base variable that the cloned tree should point back to.
 */
void lv_var_clone(lv_val *clone_var, lv_val *base_lv)
{
	lvTree		*clone_lvt;

	assert(clone_var);
	assert(LV_IS_BASE_VAR(clone_var));
	assert(base_lv);
	assert(LV_IS_BASE_VAR(base_lv));
	DBGRFCT((stderr, "\nlv_var_clone: Cloning lv_val tree at 0x"lvaddr"\n", clone_var));
	clone_lvt = LV_GET_CHILD(clone_var); /* "clone_lvt" holds tree to be cloned as we build new tree for clone_var */
	if (NULL != clone_lvt)
	{
		assert(1 == clone_lvt->sbs_depth);
		LV_TREE_CLONE(clone_lvt, (lvTreeNode *)clone_var, base_lv);
	}
}
