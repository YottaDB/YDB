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
#include "mv_stent.h"
#include "io.h"
#include "gvcst_protos.h"
#include "change_reg.h"
#include "op.h"
#include "op_tcommit.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "targ_alloc.h"
#include "gtmimagename.h"

LITREF	mval		literal_batch;

GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;

DEFINE_NSB_CONDITION_HANDLER(gvcst_spr_queryget_ch)

boolean_t	gvcst_spr_queryget(mval *cumul_val)
{
	boolean_t	spr_tpwrapped;
	boolean_t	est_first_pass;
	int		reg_index;
	gd_binding	*start_map, *end_map, *map, *prev_end_map, *stop_map;
	gd_region	*reg, *gd_reg_start;
	gd_addr		*addr;
	gv_namehead	*start_map_gvt;
	gvnh_reg_t	*gvnh_reg;
	boolean_t	found, cumul_found;
	mval		*val;
	trans_num	gd_targ_tn, *tn_array;
	char            cumul_key[MAX_KEY_SZ];
	int		cumul_key_len, prev;
#	ifdef DEBUG
	int		save_dollar_tlevel;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	start_map = TREF(gd_targ_map);	/* set up by op_gvname/op_gvnaked/op_gvextnam done just before invoking op_gvqueryget */
	start_map_gvt = gv_target;	/* save gv_target corresponding to start_map so we can restore at end */
	/* Do any initialization that is independent of retries BEFORE the op_tstart */
	PUSH_MV_STENT(MVST_MVAL);	/* Need to protect value returned by gvcst_queryget from stpgcol */
	/* "val" protection might not be necessary specifically for the non-spanning-node gvcst_queryget version.
	 * And most likely not needed for the spanning-node gvcst_queryget version. But it is not easy to be sure and
	 * besides it does not hurt that much since only a M-stack entry gets pushed. So we err on the side of caution.
	 */
	val = &mv_chain->mv_st_cont.mvs_mval;
	val->mvtype = 0; /* initialize mval in M-stack in case stp_gcol gets called before mkey gets initialized below */
	addr = TREF(gd_targ_addr);
	assert(NULL != addr);
	gd_reg_start = &addr->regions[0];
	tn_array = TREF(gd_targ_reg_array);
	gvnh_reg = TREF(gd_targ_gvnh_reg);
	assert(NULL != gvnh_reg);
	assert(NULL != gvnh_reg->gvspan);
	/* Now that we know the keyrange maps to more than one region, go through each of them and do the $queryget
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
		ESTABLISH_NORET(gvcst_spr_queryget_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		spr_tpwrapped = FALSE;
	assert(gv_cur_region == start_map->reg.addr);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* Do any initialization that is dependent on retries AFTER the op_tstart */
	map = start_map;
	cumul_found = FALSE;
	cumul_key_len = 0;
	DEBUG_ONLY(cumul_key[cumul_key_len] = KEY_DELIMITER;)
	INCREMENT_GD_TARG_TN(gd_targ_tn); /* takes a copy of incremented "TREF(gd_targ_tn)" into local variable "gd_targ_tn" */
	end_map = stop_map;
	/* Verify that initializations that happened before op_tstart are still unchanged */
	assert(addr == TREF(gd_targ_addr));
	assert(tn_array == TREF(gd_targ_reg_array));
	assert(gvnh_reg == TREF(gd_targ_gvnh_reg));
	for ( ; map <= end_map; map++)
	{	/* Scan through each map in the gld and do gvcst_queryget calls in each map's region.
		 * Note down the smallest key found across the scanned regions until we find a key that belongs to the
		 * same map (in the gld) as the currently scanned "map". At which point, the region-spanning queryget is done.
		 */
		ASSERT_BASEREG_OPEN_IF_STATSREG(map);	/* "OPEN_BASEREG_IF_STATSREG" call above should have ensured that */
		reg = map->reg.addr;
		GET_REG_INDEX(addr, gd_reg_start, reg, reg_index);	/* sets "reg_index" */
		assert((map != start_map) || (tn_array[reg_index] != gd_targ_tn));
		assert(TREF(gd_targ_reg_array_size) > reg_index);
		if (tn_array[reg_index] == gd_targ_tn)
			continue;
		if (map != start_map)
			GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
		assert(reg->open);
		if (gv_target->root)
		{
			found = gvcst_queryget(val);
			if (found)
			{
				cumul_found = TRUE;
				assert(gv_altkey->end);
				assert(KEY_DELIMITER == gv_altkey->base[gv_altkey->end]);
				assert(!cumul_key_len || (KEY_DELIMITER == cumul_key[cumul_key_len - 1]));
				if (!cumul_key_len || (0 < memcmp(&cumul_key[0], &gv_altkey->base[0], cumul_key_len)))
				{	/* just found alt_key is less (collation-wise) than cumul_key so update cumul_key */
					cumul_key_len = gv_altkey->end + 1;
					assert(cumul_key_len);
					assert(cumul_key_len < ARRAYSIZE(cumul_key));
					memcpy(&cumul_key[0], &gv_altkey->base[0], cumul_key_len);
					DEBUG_ONLY(prev_end_map = end_map;)
					/* update end_map as well now that we got an earlier cumul_key */
					end_map = gv_srch_map_linear(map, (char *)&gv_altkey->base[0], gv_altkey->end - 1);
					BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gv_altkey->base, gv_altkey->end - 1, end_map);
					assert(prev_end_map >= end_map);
					*cumul_val = *val;
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
	POP_MV_STENT();	/* "val" */
	assert(save_dollar_tlevel == dollar_tlevel);
	if (cumul_found)
	{	/* Restore accumulated minimal key into gv_altkey */
		assert(cumul_key_len);
		memcpy(&gv_altkey->base[0], &cumul_key[0], cumul_key_len);
		gv_altkey->end = cumul_key_len - 1;
		assert(KEY_DELIMITER == gv_altkey->base[gv_altkey->end]);
	}
	return cumul_found;
}
