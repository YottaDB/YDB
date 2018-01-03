/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "gvnh_spanreg.h"
#include "buddy_list.h"
#include "dpgbldir.h"
#include "change_reg.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "process_gvt_pending_list.h"	/* for "is_gvt_in_pending_list" prototype used in ADD_TO_GVT_PENDING_LIST_IF_REG_NOT_OPEN */
#include "gtmimagename.h"
#include "io.h"

GBLREF	gd_region			*gv_cur_region;

/* This assumes the input global (whose gvnh_reg_t structure is passed in as "gvnh_reg") spans multiple regions.
 * This function initializes gvnh_reg->gvspan.gvt_array[] by allocating ALL the gv_targets (if not already done)
 * 	corresponding to ALL regions that are spanned by subscripted references of the parent global name.
 * "gvnh_reg" is the gvnh_reg_t structure that has already been allocated for this global name.
 * "addr" is the corresponding gd_addr (global directory structure) whose hashtable contains "gvnh_reg"
 * If "parmblk" is non-NULL, this function is being invoked only by view_arg_convert (to set NOISOLATION status for
 *	all possible gv_targets for a given global name). And hence needs to allocate all the gvt_array[] entries
 *	even if the region is not open.
 * If "parmblk" is NULL, initialize gv_target->root as well (by opening the region if needed and doing a gvcst_root_search).
 */
void gvnh_spanreg_subs_gvt_init(gvnh_reg_t *gvnh_reg, gd_addr *addr, viewparm *parmblk)
{
	gd_binding		*gd_map_start, *map, *map_top;
	gd_region		*reg, *gd_reg_start, *save_reg;
	gv_namehead		*gvt, *name_gvt;
	gvnh_spanreg_t		*gvspan;
	int			min_reg_index, reg_index;
#	ifdef DEBUG
	gd_binding		*gd_map_top;
#	endif

	assert(NULL != gvnh_reg->gvt);
	gvspan = gvnh_reg->gvspan;
	/* Determine what regions are spanned across by this global and allocate gv_targets only for those. */
	gd_map_start = addr->maps;
	map = gd_map_start + gvspan->start_map_index;
	map_top = gd_map_start + gvspan->end_map_index + 1;
	DEBUG_ONLY(gd_map_top = &addr->maps[addr->n_maps]);
	assert(map_top <= gd_map_top);
	gd_reg_start = &addr->regions[0];
	min_reg_index = gvspan->min_reg_index;
	name_gvt = gvnh_reg->gvt;
#	ifdef DEBUG
	/* ^%Y* should never be invoked here (callers would have issued ERR_PCTYRESERVED error in that case). Assert accordingly. */
	assert((RESERVED_NAMESPACE_LEN > name_gvt->gvname.var_name.len)
		|| (0 != MEMCMP_LIT(name_gvt->gvname.var_name.addr, RESERVED_NAMESPACE)));
#	endif
	save_reg = gv_cur_region;
	for ( ; map < map_top; map++)
	{
		reg = map->reg.addr;
		GET_REG_INDEX(addr, gd_reg_start, reg, reg_index);	/* sets "reg_index" */
		assert(reg_index >= min_reg_index);
		assert(reg_index <= gvspan->max_reg_index);
		reg_index -= min_reg_index;
		gvt = gvspan->gvt_array[reg_index];
		assert(INVALID_GV_TARGET != gvt);	/* Assert that this region is indeed mapped to by the spanning global */
		if (NULL == gvt)
		{	/* If called from VIEW "NOISOLATION" (i.e. parmblk is non-NULL), do NOT open the region here.
			 * as we are going to add it to the gvt_pending_list anyways.
			 */
			if ((NULL == parmblk) && !reg->open)
				gv_init_reg(reg, NULL);
			gvt = (gv_namehead *)targ_alloc(reg->max_key_size, &name_gvt->gvname, reg);
			COPY_ACT_FROM_GVNH_REG_TO_GVT(gvnh_reg, gvt, reg);
			/* See comment in GVNH_REG_INIT macro for why the below assignment is
			 * placed AFTER all error conditions (in above macro) have passed.
			 */
			gvspan->gvt_array[reg_index] = gvt;
		}
		if (NULL != parmblk)
		{
			if (NULL != name_gvt)
				ADD_GVT_TO_VIEW_NOISOLATION_LIST(gvt, parmblk);
			/* else : view_arg_convert would have done the ADD_GVT_TO_VIEW_NOISOLATION_LIST call already */
			/* Before adding to the pending list, check if this gvt is already there
			 * (due to a previous VIEW "NOISOLATION" command. If so skip the addition.
			 */
			if (NULL == is_gvt_in_pending_list(gvt))
				ADD_TO_GVT_PENDING_LIST_IF_REG_NOT_OPEN(reg, &gvspan->gvt_array[reg_index], NULL);
		} else
			GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs/gv_target->root */
	}
	if (NULL == parmblk)
	{
		gv_cur_region = save_reg;
		change_reg();	/* restore gv_cur_region to what it was at function entry */
	}
}
