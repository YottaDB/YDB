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
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"		/* for IS_EXPLICIT_UPDATE_NOASSERT macro used by IS_OK_TO_INVOKE_GVCST_KILL macro */
#endif
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

GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;
#ifdef DEBUG
/* See comment in op_gvkill.c for why this GBLREF is necessary. */
GBLREF	jnl_gbls_t	jgbl;
#endif

DEFINE_NSB_CONDITION_HANDLER(gvcst_spr_kill_ch)

void	gvcst_spr_kill(void)
{
	boolean_t	spr_tpwrapped;
	boolean_t	est_first_pass;
	int		reg_index;
	gd_binding	*start_map, *end_map, *map;
	gd_region	*reg, *gd_reg_start;
	gd_addr		*addr;
	gv_namehead	*start_map_gvt;
	gvnh_reg_t	*gvnh_reg;
	trans_num	gd_targ_tn, *tn_array;
#	ifdef DEBUG
	int		save_dollar_tlevel;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	start_map = TREF(gd_targ_map);	/* set up by op_gvname/op_gvnaked/op_gvextnam done just before invoking op_gvkill */
	start_map_gvt = gv_target;	/* save gv_target corresponding to start_map so we can restore at end */
	/* Find out if the next (in terms of $order) key maps to same map as currkey. If so, no spanning activity needed */
	GVKEY_INCREMENT_ORDER(gv_currkey);
	end_map = gv_srch_map_linear(start_map, (char *)&gv_currkey->base[0], gv_currkey->end - 1);
	BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gv_currkey->base, gv_currkey->end - 1, end_map);
	GVKEY_UNDO_INCREMENT_ORDER(gv_currkey);
	if (start_map == end_map)
	{
		assert(gv_target == start_map_gvt);
		if (IS_OK_TO_INVOKE_GVCST_KILL(start_map_gvt))
			gvcst_kill(TRUE);
		return;
	}
	/* Do any initialization that is independent of retries BEFORE the op_tstart */
	addr = TREF(gd_targ_addr);
	assert(NULL != addr);
	gd_reg_start = &addr->regions[0];
	tn_array = TREF(gd_targ_reg_array);
	gvnh_reg = TREF(gd_targ_gvnh_reg);
	assert(NULL != gvnh_reg);
	assert(NULL != gvnh_reg->gvspan);
	/* Now that we know the keyrange maps to more than one region, go through each of them and do the kill.
	 * Since multiple regions are potentially involved, need a TP fence.
	 */
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	if (!dollar_tlevel)
	{
		spr_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART), TRUE, &literal_batch, 0);
		ESTABLISH_NORET(gvcst_spr_kill_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		spr_tpwrapped = FALSE;
	assert(gv_cur_region == start_map->reg.addr);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* Do any initialization that is dependent on retries AFTER the op_tstart */
	map = start_map;
	INCREMENT_GD_TARG_TN(gd_targ_tn); /* takes a copy of incremented "TREF(gd_targ_tn)" into local variable "gd_targ_tn" */
	/* Verify that initializations that happened before op_tstart are still unchanged */
	assert(addr == TREF(gd_targ_addr));
	assert(tn_array == TREF(gd_targ_reg_array));
	assert(gvnh_reg == TREF(gd_targ_gvnh_reg));
	for ( ; map <= end_map; map++)
	{
		ASSERT_BASEREG_OPEN_IF_STATSREG(map);	/* "gv_srch_map_linear" call above should have ensured that */
		reg = map->reg.addr;
		GET_REG_INDEX(addr, gd_reg_start, reg, reg_index);	/* sets "reg_index" */
		assert((map != start_map) || (tn_array[reg_index] != gd_targ_tn));
		assert(TREF(gd_targ_reg_array_size) > reg_index);
		if (tn_array[reg_index] == gd_targ_tn)
			continue;
		if (map != start_map)
			GV_BIND_SUBSREG(addr, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
		assert(reg->open);
		if (IS_OK_TO_INVOKE_GVCST_KILL(gv_target))
			gvcst_kill(TRUE);
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
	return;
}
