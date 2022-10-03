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

DEFINE_NSB_CONDITION_HANDLER(gvcst_spr_query_ch)

boolean_t	gvcst_spr_query(void)
{
	boolean_t	spr_tpwrapped;
	boolean_t	est_first_pass;
	boolean_t	found, result_found;
	boolean_t	do_atorder_search = FALSE;
	boolean_t	currkey_saved = FALSE;
	int		reg_index;
	gd_binding	*start_map, *end_map, *map, *prev_end_map, *stop_map, *rmap;
	gd_region	*reg, *gd_reg_start;
	gd_addr		*addr;
	gv_namehead	*start_map_gvt;
	gv_key		*matchkey = NULL;
	gv_key_buf	currkey_save_buf = { .key.top = DBKEYSIZE(MAX_KEY_SZ), .key.end = 0, .key.prev = 0 };
	gvnh_reg_t	*gvnh_reg;
	mname_entry	gvname;
	srch_blk_status	*bh;
	trans_num	gd_targ_tn, *tn_array;
#	ifdef DEBUG
	int		save_dollar_tlevel;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	start_map = TREF(gd_targ_map);	/* set up by op_gvname/op_gvnaked/op_gvextnam done just before invoking op_gvquery */
	start_map_gvt = gv_target;	/* save gv_target corresponding to start_map so we can restore at end */
	/* Do any initialization that is independent of retries BEFORE the op_tstart */
	addr = TREF(gd_targ_addr);
	assert(NULL != addr);
	gd_reg_start = &addr->regions[0];
	tn_array = TREF(gd_targ_reg_array);
	gvnh_reg = TREF(gd_targ_gvnh_reg);
	assert(NULL != gvnh_reg);
	assert(NULL != gvnh_reg->gvspan);
	/* Now that we know the keyrange maps to more than one region, go through each of them and do the $query
	 * Since multiple regions are potentially involved, need a TP fence. But before that, open any statsDBs pointed
	 * to by map entries from "start_map" to "stop_map" (as their open will be deferred once we go into TP). Not
	 * opening them before the TP can produce incomplete results from the $query operation.
	 */
	assert(0 < gvnh_reg->gvspan->end_map_index);
	assert(gvnh_reg->gvspan->end_map_index < addr->n_maps);
	stop_map = &addr->maps[gvnh_reg->gvspan->end_map_index];
	for (map = start_map; map <= stop_map; map++)
		OPEN_BASEREG_IF_STATSREG(map);
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	if (!dollar_tlevel)
	{
		spr_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_spr_query_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		spr_tpwrapped = FALSE;
	assert(gv_cur_region == start_map->reg.addr);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* Do any initialization that is dependent on retries AFTER the op_tstart */
	map = start_map;
	result_found = FALSE;
	do_atorder_search = FALSE;
	INCREMENT_GD_TARG_TN(gd_targ_tn); /* takes a copy of incremented "TREF(gd_targ_tn)" into local variable "gd_targ_tn" */
	end_map = stop_map;
	/* Verify that initializations that happened before op_tstart are still unchanged */
	assert(addr == TREF(gd_targ_addr));
	assert(tn_array == TREF(gd_targ_reg_array));
	assert(gvnh_reg == TREF(gd_targ_gvnh_reg));
	for ( ; map <= end_map; map++)
	{	/* Scan through each map in the gld and do gvcst_query calls in each map's region.
		 * Note down the smallest key found across the scanned regions until we find a key that belongs to the
		 * same map (in the gld) as the currently scanned "map". At which point, the region-spanning query is done.
		 */
		ASSERT_BASEREG_OPEN_IF_STATSREG(map);	/* "OPEN_BASEREG_IF_STATSREG" call above should have ensured that */
		reg = map->reg.addr;
		GET_REG_INDEX(addr, gd_reg_start, reg, reg_index);	/* sets "reg_index" */
		assert((map != start_map) || (tn_array[reg_index] != gd_targ_tn));
		assert(TREF(gd_targ_reg_array_size) > reg_index);
		if ((map != start_map) || (gv_target->gd_csa != cs_addrs) || (gv_cur_region != start_map->reg.addr))
			GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
		assert(reg->open);
		if (gv_target->root)
		{
			matchkey = gv_altkey;
			found = gvcst_query();
			if (do_atorder_search)
			{
				bh = gv_target->hist.h;
				if ((gv_currkey->end <= bh->curr_rec.match) && (1 == (gvcst_data2() % 10)))
				{	/* The history indicates that the gvcst_search() performed by gvcst_query() actually found
					 * the key marking the end of a previous map, so use that.
					 */
					matchkey = gv_currkey;
					found = TRUE;
				}
			}
			if (found)
			{
				rmap = gv_srch_map_linear(map, (char *)&matchkey->base[0], matchkey->end - 1);
				if (map == rmap)
				{
					assert(matchkey->end);
					assert(KEY_DELIMITER == matchkey->base[matchkey->end]);
					result_found = TRUE;
					break;
				}
			}
		}
		tn_array[reg_index] = gd_targ_tn;
		/* Since we know that any subsequent match would lie beyond the current map, replace the last subscript
		 * in the search with the corresponding one from the map endpoint.
		 * This keeps us from getting false hits which would have been associated with earlier maps.
		 */
		if (map != end_map)
		{
			if (!currkey_saved)
			{
				COPY_KEY(&currkey_save_buf.key, gv_currkey);
				currkey_saved = TRUE;
			}
			assert(!strcmp((char *)gv_currkey->base, map->gvkey.addr));
			memcpy(gv_currkey->base, map->gvkey.addr, map->gvkey_len);
			gv_currkey->end = map->gvkey_len;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
			gv_currkey->base[gv_currkey->end + 1] = KEY_DELIMITER;
			do_atorder_search = TRUE;
		}
	}
	if (result_found)
	{
		assert(matchkey);
		if (gv_altkey != matchkey)
			COPY_KEY(gv_altkey, matchkey);
	}
	if (currkey_saved)
		COPY_KEY(gv_currkey, &currkey_save_buf.key);
	if (currkey_saved || (gv_target != start_map_gvt))
	{	/* Restore gv_cur_region/gv_target etc. */
		gv_target = start_map_gvt;
		gv_cur_region = start_map->reg.addr;
		change_reg();
	}
	assert(gv_cur_region == start_map->reg.addr);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	if (spr_tpwrapped)
	{
		op_tcommit();
		REVERT; /* remove our condition handler */
	}
	assert(save_dollar_tlevel == dollar_tlevel);
	return result_found;
}
