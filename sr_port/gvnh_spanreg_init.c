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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "gvnh_spanreg.h"
#include "gvcst_protos.h"

/* Initialize gvnh_reg->gvspan if input global spans multiple regions.
 * "gvnh_reg" is the gvnh_reg_t structure that has already been allocated for this global name.
 * "addr" is the corresponding gd_addr (global directory structure) whose hashtable contains "gvnh_reg"
 * "gvmap_start" is the map entry in the gld file where the unsubscripted global name maps to.
 */
void gvnh_spanreg_init(gvnh_reg_t *gvnh_reg, gd_addr *addr, gd_binding *gvmap_start)
{
	gvnh_spanreg_t	*gvspan;
	gd_binding	*gvmap_end, *gdmap_start;
	mident		*gvname;
	char		*gvent_name, *c, *c_top;
	int		gvent_len, res, reg_index, gvspan_size;
	unsigned int	min_reg_index, max_reg_index;
	gd_region	*reg, *reg_start;
#	ifdef DEBUG
	boolean_t	min_reg_index_adjusted = FALSE;
	gd_region	*reg_top;
	gd_binding	*gdmap_top;
	trans_num	gd_targ_tn, *tn_array;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* At this point "gvnh_reg->gd_reg" and "gvnh_reg->gvt" have already been initialized */
	/* First check if global spans multiple regions */
	gvname = &gvnh_reg->gvt->gvname.var_name;
	gvent_len = gvname->len;
	gvent_name = gvname->addr;
	/* Assert that gvname is not subscripted (i.e. does not contain null bytes) */
#	ifdef DEBUG
	c = gvent_name;
	c_top = gvent_name + gvent_len;
	for ( ; c < c_top; c++)
		assert(KEY_DELIMITER != *c);
#	endif
	gvmap_end = gvmap_start;
	if (!TREF(no_spangbls))
	{
		reg_start = addr->regions;
		min_reg_index = addr->n_regions;	/* impossible value of index into addr->regions[] array */
		DEBUG_ONLY(reg_top = reg_start + min_reg_index;)
		max_reg_index = 0;
		DEBUG_ONLY(INCREMENT_GD_TARG_TN(gd_targ_tn);)	/* takes a copy of incremented "TREF(gd_targ_tn)"
								 * into local variable "gd_targ_tn" */
		DEBUG_ONLY(tn_array = TREF(gd_targ_reg_array);)	/* could be NULL if no spanning globals were seen till now */
		for ( ; ; gvmap_end++)
		{
			res = memcmp(gvent_name, &(gvmap_end->gvkey.addr[0]), gvent_len);
			assert(0 >= res);
			reg = gvmap_end->reg.addr;
			GET_REG_INDEX(addr, reg_start, reg, reg_index);	/* sets "reg_index" */
#			ifdef DEBUG
			if (NULL != tn_array)
				tn_array[reg_index] = gd_targ_tn;
#			endif
			if (min_reg_index > reg_index)
				min_reg_index = reg_index;
			if (max_reg_index < reg_index)
				max_reg_index = reg_index;
			assert((0 != res) || (gvent_len <= gvmap_end->gvname_len));
			if ((0 > res) || ((0 == res) && (gvent_len < gvmap_end->gvname_len)))
				break;
		}
	}
	/* else : no_spangbls is TRUE which means this process does not need to worry about globals spanning multiple regions */
	if (gvmap_end == gvmap_start)
	{	/* global does not span multiple regions. */
		gvnh_reg->gvspan = NULL;
		return;
	}
	TREF(spangbl_seen) = TRUE;	/* we found at least one global that spans multiple regions */
	/* If global name is ^%YGS, the map entries for ^%YGS might change with time (see OPEN_BASEREG_IF_STATSREG macro
	 * AND ygs_map->reg.addr in "gvcst_init_statsDB"). So keep min_reg_index as lowest possible value.
	 */
	if ((gvent_len == STATSDB_GBLNAME_LEN) && (0 == memcmp(gvent_name, STATSDB_GBLNAME, STATSDB_GBLNAME_LEN)))
	{
		DEBUG_ONLY(min_reg_index_adjusted = TRUE;)
		min_reg_index = 0;
	}
	/* Allocate and initialize a gvnh_spanreg_t structure and link it to gvnh_reg.
	 * Note gvt_array[] size is max_reg_index - min_reg_index + 1.
	 */
	gvspan_size = SIZEOF(gvspan->gvt_array[0]) * (max_reg_index - min_reg_index);
	gvspan = (gvnh_spanreg_t *)malloc(SIZEOF(gvnh_spanreg_t) + gvspan_size);
	gvspan->min_reg_index = min_reg_index;
	gvspan->max_reg_index = max_reg_index;
	gdmap_start = addr->maps;
	DEBUG_ONLY(gdmap_top = gdmap_start + addr->n_maps;)
	assert((gvmap_start >= gdmap_start) && (gvmap_start < gdmap_top));
	assert((gvmap_end >= gdmap_start) && (gvmap_end < gdmap_top));
	gvspan->start_map_index = gvmap_start - gdmap_start;
	gvspan->end_map_index = gvmap_end - gdmap_start;
	/* Initialize the array of gv_targets (corresponding to each spanned region) to NULL initially.
	 * As and when each region is referenced, the gv_targets will get allocated.
	 */
	memset(&gvspan->gvt_array[0], 0, gvspan_size + SIZEOF(gvspan->gvt_array[0]));
#	ifdef DEBUG
	if (!min_reg_index_adjusted)
	{	/* Initialize the region slots that are not spanned to by this global with a distinct "invalid" value */
		assert(tn_array[min_reg_index] == gd_targ_tn);
		assert(tn_array[max_reg_index] == gd_targ_tn);
		for (reg_index = min_reg_index; reg_index <= max_reg_index; reg_index++)
		{
			if (tn_array[reg_index] != gd_targ_tn)
				gvspan->gvt_array[reg_index - min_reg_index] = INVALID_GV_TARGET;
		}
	}
#	endif
	gvnh_reg->gvspan = gvspan;
	/* Initialize gvt for the region that the unsubscripted global name maps to */
	reg = gvmap_start->reg.addr;
	GET_REG_INDEX(addr, reg_start, reg, reg_index);	/* sets "reg_index" */
	assert((reg_index >= min_reg_index) && (reg_index <= max_reg_index));
	assert(INVALID_GV_TARGET != gvspan->gvt_array[reg_index - min_reg_index]);	/* Assert that this region is indeed
											 * mapped to by the spanning global */
	gvspan->gvt_array[reg_index - min_reg_index] = gvnh_reg->gvt;
	return;
}
