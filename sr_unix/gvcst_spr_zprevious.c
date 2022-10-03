/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "gvt_inline.h"

LITREF	mval		literal_batch;

GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;

DEFINE_NSB_CONDITION_HANDLER(gvcst_spr_zprevious_ch)

boolean_t	gvcst_spr_zprevious(void)
{
	boolean_t	spr_tpwrapped;
	boolean_t	est_first_pass;
	boolean_t	found, result_found;
	boolean_t	do_atorder_search;
	boolean_t	currkey_orig_saved = FALSE;
	char		savech;
	int		reg_index;
	int		prev;
	gd_binding	*start_map, *first_map, *stop_map, *end_map, *map, *prev_end_map, *rmap;
	gd_region	*reg, *gd_reg_start;
	gd_addr		*addr;
	gv_namehead	*start_map_gvt;
	gv_key		*matchkey = NULL;
	gv_key_buf	currkey_save_buf = { .key.top = DBKEYSIZE(MAX_KEY_SZ), .key.end = 0, .key.prev = 0 };
	gv_key_buf	currkey_orig_buf = { .key.top = DBKEYSIZE(MAX_KEY_SZ), .key.end = 0, .key.prev = 0 };
	gv_key_buf	altkey_save_buf = { .key.top = DBKEYSIZE(MAX_KEY_SZ), .key.end = 0, .key.prev = 0 };
	gvnh_reg_t	*gvnh_reg;
	srch_blk_status	*bh;
	trans_num	gd_targ_tn, *tn_array;
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
		if ((start_map != first_map) || (gv_target->gd_csa != cs_addrs) || (gv_cur_region != start_map->reg.addr))
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
	result_found = FALSE;
	do_atorder_search = FALSE;
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
		assert(TREF(gd_targ_reg_array_size) > reg_index);
		GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
		assert(reg->open);
		if (gv_target->root)
		{
			matchkey = gv_altkey;
			found = gvcst_zprevious();
			assert(!found || !memcmp(&gv_altkey->base[0], &gv_currkey->base[0], prev));
			if (do_atorder_search)
			{
				bh = gv_target->hist.h;
				if (gv_currkey->end <= bh->curr_rec.match)
				{	/* The history indicates that the gvcst_search() performed by gvcst_order() actually found
					 * the key marking the end of a previous map, so use that.
					 */
					matchkey = gv_currkey;
					found = TRUE;
				}
			}
			if (found)
			{	/* For accurate results, we need to see if the node found by gvcst_zprevious() has data or that its
				 * first data child lies in the same map. Otherwise, the node should be excluded.
				 * To do this, take the proposed key and do a gvcst_query() on it.
				 * If the resulting key matches the request and is in the current map, use it to derive the result.
				 * Otherwise, put things back to where they were after the search and continue.
				 */
				if (!currkey_orig_saved)
				{
					COPY_KEY(&currkey_orig_buf.key, gv_currkey);
					currkey_orig_saved = TRUE;
				}
				COPY_KEY(&currkey_save_buf.key, gv_currkey);
				if (matchkey == gv_altkey)
				{
					COPY_KEY(gv_currkey, gv_altkey);
					gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
				} else
				{
					assert(matchkey == gv_currkey);
					COPY_KEY(&altkey_save_buf.key, gv_altkey);
					altkey_save_buf.split.base[altkey_save_buf.key.end] = KEY_DELIMITER;
					if ((map[-1].gvkey_len > gv_currkey->end)
							&& (0 == memcmp(gv_currkey->base, map[-1].gvkey.addr, gv_currkey->end)))
					{	/* The prior map endpoint has the same subscript as the one we are looking for,
						 * so do query search from there in order to avoid incorrectly matching earlier
						 * next level subscripts.
						 */
						memcpy(gv_currkey->base, map[-1].gvkey.addr, map[-1].gvkey_len);
						gv_currkey->end = map[-1].gvkey_len;
						gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
					}
				}
				do
				{
					found = gvcst_query2();
					/* The query result is only useful if it shares the same base as the currkey
					 * and has subscripts beyond that.
					 */
					if (found)
					{
						assert(gv_altkey->end >= prev);
						assert(0 == memcmp(gv_currkey->base, gv_altkey->base, prev));
						rmap = gv_srch_map_linear_backward(map, (char *)gv_altkey->base,
											gv_altkey->end - 1);
						if (map == rmap)
						{
							result_found = TRUE;
							/* Use altkey from query, but only up to the subscript we care about. */
							for (gv_altkey->end = prev;
									gv_altkey->base[gv_altkey->end];
									gv_altkey->end++)
								;
							gv_altkey->base[++gv_altkey->end] = KEY_DELIMITER;
							/* gv_currkey will be restored from currkey_orig on the way out,
							 * so no need to restore it here.
							 */
							matchkey = gv_altkey;
							break;
						}
					}
					if (matchkey == gv_altkey)
					{	/* The saved gv_altkey is a miss, so don't bother restoring it. */
						COPY_KEY(gv_currkey, &currkey_save_buf.key);
						matchkey = NULL;
					} else
					{	/* The currkey check failed, so try the altkey. */
						COPY_KEY(gv_currkey, &altkey_save_buf.key);
						assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
						matchkey = gv_altkey;
					}
				} while (matchkey);
				if (matchkey)
				{	/* Only non-NULL if we did a break out of the above loop. */
					break;
				}
			}
		}
		if (map != end_map)
		{	/* Since we know that any subsequent match would lie before the current map,
			 * replace the last subscript in the search with the corresponding one from
			 * the previous map endpoint.
			 * This keeps us from getting false hits which would have been associated with earlier maps.
			 */
			assert(!memcmp(map[-1].gvkey.addr, &gv_currkey->base[0], prev));
			assert(0 >= memcmp(map[-1].gvkey.addr + prev, gv_currkey->base + prev, gv_currkey->end - prev));
			if (!currkey_orig_saved)
			{
				COPY_KEY(&currkey_orig_buf.key, gv_currkey);
				currkey_orig_saved = TRUE;
			}
			for (gv_currkey->end = prev; map[-1].gvkey.addr[gv_currkey->end]; gv_currkey->end++)
			{
				gv_currkey->base[gv_currkey->end] = map[-1].gvkey.addr[gv_currkey->end];
			}
			gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
			do_atorder_search = TRUE;
		} else
			do_atorder_search = FALSE;
		tn_array[reg_index] = gd_targ_tn;
	}
	if (currkey_orig_saved || (gv_target != start_map_gvt))
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
	if (result_found)
	{	/* Restore accumulated minimal key into gv_altkey */
		assert(matchkey);
		if (gv_altkey != matchkey)
			COPY_KEY(gv_altkey, matchkey);
	}
	if (currkey_orig_saved)
		COPY_KEY(gv_currkey, &currkey_orig_buf.key);
	return result_found;
}
