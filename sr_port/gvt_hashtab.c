/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "process_gvt_pending_list.h"
#include "hashtab_mname.h"
#include "targ_alloc.h"
#include "gvt_hashtab.h"

GBLREF	gv_namehead	*gv_target_list;

void gvt_hashtab_init(sgmnt_addrs *csa)
{
	gv_namehead	*gvtarg;
	boolean_t	added;
	ht_ent_mname	*stayent;

	assert(NULL == csa->gvt_hashtab);
	/* This is the first time a duplicate region for the same database file is being opened.
	 * Since two regions point to the same physical file, start maintaining a list of all global variable
	 * names whose gv_targets have already been allocated on behalf of the current database file (not including
	 * the region that is currently being opened for which gvt->gd_csa will still be NULL).
	 * Future targ_allocs will check this list before they allocate (to avoid duplicate allocations).
	 */
	csa->gvt_hashtab = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname(csa->gvt_hashtab, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	assert(1 == csa->regcnt);
	for (gvtarg = gv_target_list; NULL != gvtarg; gvtarg = gvtarg->next_gvnh)
	{	/* There is one region that is "open" and has gv_targets allocated for this "csa".
		 * Add those gv_targets into the hashtable first.
		 */
		if (gvtarg->gd_csa != csa)
			continue;
		if (DIR_ROOT == gvtarg->root)
			continue;	/* gvt is csa->dir_tree and does not correspond to a global name */
		added = add_hashtab_mname(csa->gvt_hashtab, &gvtarg->gvname, gvtarg, &stayent);
		assert(added && (1 == gvtarg->regcnt));
	}
}
