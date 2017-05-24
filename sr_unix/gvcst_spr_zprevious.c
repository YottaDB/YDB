/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */
#include "io.h"
#include "gvcst_protos.h"
#include "change_reg.h"
#include "op.h"
#include "op_tcommit.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "targ_alloc.h"
#include "error.h"
#include "stack_frame.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;

GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;

DEFINE_NSB_CONDITION_HANDLER(gvcst_spr_zprevious_ch)

boolean_t	gvcst_spr_zprevious(void)
{
	boolean_t	spr_tpwrapped;
	boolean_t	est_first_pass;
	boolean_t	found, cumul_found;
	int		reg_index;
	gd_binding	*start_map, *first_map, *stop_map, *end_map, *map, *prev_end_map;
	gd_region	*reg, *gd_reg_start;
	gd_addr		*addr;
	gv_namehead	*start_map_gvt;
	gvnh_reg_t	*gvnh_reg;
	trans_num	gd_targ_tn, *tn_array;
	char            cumul_key[MAX_KEY_SZ], savech;
	int		cumul_key_len, prev;
#	ifdef DEBUG
	int		save_dollar_tlevel;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Ensure gv_cur_region/gv_target/cs_addrs at function return are identical to values at function entry
	 * (this is not currently required now that op_gvname fast-path optimization has been removed in GTM-2168
	 * but might be better to hold onto just in case that optimization is resurrected for non-spanning-globals or so)
	 */
	start_map = TREF(gd_targ_map);	/* set up by op_gvname/op_gvnaked/op_gvextnam done just before invoking op_zprevious */
	start_map_gvt = gv_target;
	addr = TREF(gd_targ_addr);
	assert(NULL != addr);
	gvnh_reg = TREF(gd_targ_gvnh_reg);
	assert(NULL != gvnh_reg);
	assert(NULL != gvnh_reg->gvspan);
	if (TREF(gv_last_subsc_null))
	{	/* last subscript is "". gv_currkey would have been recomputed in op_zprevious to have a 0xFF byte sequence
		 * reflecting the maximum possible subscript value. Since this global spans multiple regions, recompute
		 * the map corresponding to this gv_currkey.
		 */
		first_map = gv_srch_map_linear(start_map, (char *)&gv_currkey->base[0], gv_currkey->end - 1);
	} else
	{
		first_map = start_map;
		BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gv_currkey->base, gv_currkey->end - 1, first_map);
		if (start_map != first_map)
		{	/* set global variables to point to new first_map region */
			reg = first_map->reg.addr;
			GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs to new first_map */
		}
	}
	/* Check if the previous key at SAME subscript level maps to same map as currkey. If so, no spanning activity needed */
	GVKEY_SET_SUBS_ZPREVIOUS(gv_currkey, savech);
	prev = gv_currkey->prev;
	stop_map = gv_srch_map_linear_backward(first_map, (char *)&gv_currkey->base[0], prev + 1);
	assert(stop_map <= first_map);
	GVKEY_UNDO_SET_SUBS_ZPREVIOUS(gv_currkey, savech);
	found = FALSE;
	if (first_map == stop_map)
	{	/* At this point, gv_target could be different from start_map_gvt.
		 * Hence cannot use latter like is used in other gvcst_spr_* modules.
		 */
		if (gv_target->root)
			found = gvcst_zprevious();
		if (gv_target != start_map_gvt)
		{	/* Restore gv_cur_region/gv_target etc. */
			gv_target = start_map_gvt;
			gv_cur_region = start_map->reg.addr;
			change_reg();
		}
		return found;
	}
	/* Do any initialization that is independent of retries BEFORE the op_tstart */
	gd_reg_start = &addr->regions[0];
	tn_array = TREF(gd_targ_reg_array);
	/* Now that we know the keyrange maps to more than one region, go through each of them and do the $zprevious
	 * Since multiple regions are potentially involved, need a TP fence.
	 */
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	if (!dollar_tlevel)
	{
		spr_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_spr_zprevious_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		spr_tpwrapped = FALSE;
	assert((first_map != start_map) || (gv_cur_region == first_map->reg.addr));
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* Do any initialization that is dependent on retries AFTER the op_tstart */
	map = first_map;
	end_map = stop_map;
	cumul_found = FALSE;
	cumul_key_len = 0;
	DEBUG_ONLY(cumul_key[cumul_key_len] = KEY_DELIMITER;)
	INCREMENT_GD_TARG_TN(gd_targ_tn); /* takes a copy of incremented "TREF(gd_targ_tn)" into local variable "gd_targ_tn" */
	/* Verify that initializations that happened before op_tstart are still unchanged */
	assert(addr == TREF(gd_targ_addr));
	assert(tn_array == TREF(gd_targ_reg_array));
	assert(gvnh_reg == TREF(gd_targ_gvnh_reg));
	for ( ; map >= end_map; map--)
	{	/* Backward scan through each map in the gld and do gvcst_zprevious calls in each map's region.
		 * Note down the largest key found across the scanned regions until we find a key that belongs to the
		 * same map (in the gld) as the currently scanned "map". At which point, the region-spanning zprevious is done.
		 */
		ASSERT_BASEREG_OPEN_IF_STATSREG(map);	/* "gv_srch_map_linear" and "gv_srch_map_linear_backward" call above
							 * should have ensured that.
							 */
		reg = map->reg.addr;
		GET_REG_INDEX(addr, gd_reg_start, reg, reg_index);	/* sets "reg_index" */
		assert((map != first_map) || (tn_array[reg_index] != gd_targ_tn));
		assert(TREF(gd_targ_reg_array_size) > reg_index);
		if (tn_array[reg_index] == gd_targ_tn)
			continue;
		GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
		assert(reg->open);
		if (gv_target->root)
		{
			found = gvcst_zprevious();
			if (found)
			{
				assert(!memcmp(&gv_altkey->base[0], &gv_currkey->base[0], prev));
				cumul_found = TRUE;
				assert(gv_altkey->end);
				assert(gv_altkey->end > prev);
				assert(KEY_DELIMITER == gv_altkey->base[gv_altkey->end]);
				assert(!cumul_key_len || (KEY_DELIMITER == cumul_key[cumul_key_len - 1]));
				if (!cumul_key_len || (0 > memcmp(&cumul_key[0], &gv_altkey->base[prev], cumul_key_len)))
				{	/* just found alt_key is greater (collation-wise) than cumul_key so update cumul_key */
					cumul_key_len = gv_altkey->end - prev;
					assert(cumul_key_len);
					assert(cumul_key_len < ARRAYSIZE(cumul_key));
					memcpy(&cumul_key[0], &gv_altkey->base[prev], cumul_key_len);
					DEBUG_ONLY(prev_end_map = end_map;)
					/* update end_map as well now that we got a bigger cumul_key */
					end_map = gv_srch_map_linear_backward(map, (char *)&gv_altkey->base[0], gv_altkey->end - 1);
					assert(prev_end_map <= end_map);
				}
			}
		}
		tn_array[reg_index] = gd_targ_tn;
	}
	if (gv_target != start_map_gvt)
	{	/* Restore gv_cur_region/gv_target etc. */
		gv_target = start_map_gvt;
		gv_cur_region = start_map->reg.addr;
		change_reg();
	}
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	if (spr_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	assert(save_dollar_tlevel == dollar_tlevel);
	if (cumul_found)
	{	/* Restore accumulated minimal key into gv_altkey */
		assert(cumul_key_len);
		memcpy(&gv_altkey->base[0], &gv_currkey->base[0], prev);
		memcpy(&gv_altkey->base[prev], &cumul_key[0], cumul_key_len);
		gv_altkey->end = prev + cumul_key_len;
		gv_altkey->base[gv_altkey->end] = KEY_DELIMITER;
	}
	return cumul_found;
}
