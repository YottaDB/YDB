/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_order,gvcst_data prototype */
#include "change_reg.h"
#include "gvsub2str.h"
#include "gvcmx.h"
#include "gvusr.h"
#include "hashtab_mname.h"
#include "targ_alloc.h"		/* for GV_BIND_SUBSREG macro which needs "targ_alloc" prototype */
#include "gtmimagename.h"
#include "mvalconv.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF spdesc		stringpool;

/* op_zprevious should generally be maintained in parallel */

void op_gvorder(mval *v)
{
	int4			n;
	int			min_reg_index, reg_index, res;
	int			i, mini, maxi;
	mname_entry		gvname;
	mval			tmpmval, *datamval;
	enum db_acc_method	acc_meth;
	boolean_t		found, ok_to_change_currkey;
	gd_binding		*gd_map_start, *map, *map_top;
	gd_addr			*gd_targ;
	gv_namehead		*gvt;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	acc_meth = REG_ACC_METH(gv_cur_region);
	/* Modify gv_currkey such that a gvcst_search of the resulting gv_currkey will find the next available subscript.
	 * But in case of dba_usr (the custom implementation of $ORDER which is overloaded for DDP but could be more in the
	 * future) it is better to hand over gv_currkey as it is so the custom implementation can decide what to do with it.
	 */
	ok_to_change_currkey = (dba_usr != acc_meth);
	if (ok_to_change_currkey)
	{	/* Modify gv_currkey to reflect the next possible key value in collating order */
		if (!TREF(gv_last_subsc_null) || gv_cur_region->std_null_coll)
		{
			GVKEY_INCREMENT_ORDER(gv_currkey);
		} else
		{
			assert(STR_SUB_PREFIX == gv_currkey->base[gv_currkey->prev]);
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
			assert(2 == (gv_currkey->end - gv_currkey->prev));
			*(&gv_currkey->base[0] + gv_currkey->prev) = 01;
		}
	}
	if (gv_currkey->prev)
	{
		if ((dba_bg == acc_meth) || (dba_mm == acc_meth))
		{
			gvnh_reg = TREF(gd_targ_gvnh_reg);
			if (NULL == gvnh_reg)
				found = (gv_target->root ? gvcst_order() : FALSE);
			else
				INVOKE_GVCST_SPR_XXX(gvnh_reg, found = gvcst_spr_order());
		} else if (dba_cm == acc_meth)
			found = gvcmx_order();
		else
			found = gvusr_order();
		v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied by (BYPASSOK)
				* this to-be-overwritten mval */
		if (found)
		{
			gv_altkey->prev = gv_currkey->prev;
			if (!(IS_STP_SPACE_AVAILABLE(MAX_KEY_SZ)))
			{
				if (*(&gv_altkey->base[0] + gv_altkey->prev) != 0xFF)
					n = MAX_FORM_NUM_SUBLEN;
				else
				{
					n = gv_altkey->end - gv_altkey->prev;
					assert(n > 0);
				}
				ENSURE_STP_FREE_SPACE(n);
			}
			v->str.addr = (char *)stringpool.free;
			stringpool.free = gvsub2str (&gv_altkey->base[0] + gv_altkey->prev, stringpool.free, FALSE);
			v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		if (ok_to_change_currkey)
		{	/* Restore gv_currkey to what it was at function entry time */
			if (!TREF(gv_last_subsc_null) || gv_cur_region->std_null_coll)
			{
				assert(1 == gv_currkey->base[gv_currkey->end - 2]);
				assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end-1]);
				assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
				gv_currkey->base[gv_currkey->end - 2] = KEY_DELIMITER;
				gv_currkey->end--;
			} else
			{
				assert(01 == gv_currkey->base[gv_currkey->prev]);
				gv_currkey->base[gv_currkey->prev] = STR_SUB_PREFIX;
			}
		}
	} else	/* the following section is for $O(^gname) */
	{
		assert(2 < gv_currkey->end);
		assert(gv_currkey->end < (MAX_MIDENT_LEN + 3));	/* until names are not in midents */
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
		gd_targ = TREF(gd_targ_addr);
		gd_map_start = gd_targ->maps;
		map_top = gd_map_start + gd_targ->n_maps;
		map = gv_srch_map(gd_targ, (char *)&gv_currkey->base[0], gv_currkey->end - 1);
		for ( ; map < map_top; ++map)
		{
			gv_cur_region = map->reg.addr;
			if (!gv_cur_region->open)
				gv_init_reg(gv_cur_region);
			change_reg();
			acc_meth = REG_ACC_METH(gv_cur_region);
			/* search region, entries in directory tree could have empty GVT in which case move on to next entry */
			for ( ; ; )
			{
				if ((dba_bg == acc_meth) || (dba_mm == acc_meth))
				{
					gv_target = cs_addrs->dir_tree;
					found = gvcst_order();
				} else if (acc_meth == dba_cm)
					found = gvcmx_order();
				else
					found = gvusr_order();
				if (!found)
					break;
				/* At this point, gv_altkey contains the result of the gvcst_order */
				assert(1 < gv_altkey->end);
				assert((MAX_MIDENT_LEN + 2) > gv_altkey->end);	/* until names are not in midents */
				res = memcmp(gv_altkey->base, map->gvkey.addr, gv_altkey->end);
				assert((0 != res) || (gv_altkey->end <= map->gvkey_len));
				if ((0 < res) || ((0 == res) && (map->gvkey_len == (map->gvname_len + 1))))
				{	/* The global name we found is greater than the maximum value in this map OR
					 * it is exactly equal to the upper bound of this map so this name cannot be
					 * found in current map for sure. Move on to next map.
					 */
					found = FALSE;
					break;
				}
				gvname.var_name.addr = (char *)&gv_altkey->base[0];
				gvname.var_name.len = gv_altkey->end - 1;
				if (dba_cm == acc_meth)
					break;
				COMPUTE_HASH_MNAME(&gvname);
				GV_BIND_NAME_AND_ROOT_SEARCH(gd_targ, &gvname, gvnh_reg);	/* updates "gv_currkey" */
				gvspan = gvnh_reg->gvspan;
				assert((NULL != gvspan) || (gv_cur_region == map->reg.addr));
				if (NULL != gvspan)
				{	/* gv_target would NOT have been initialized by GV_BIND_NAME in this case.
					 * So finish that initialization.
					 */
					datamval = &tmpmval;
					/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH
					 * 	(e.g. setting gv_cur_region for spanning globals)
					 */
					GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_targ, gv_currkey, gvnh_reg->gd_reg);
					op_gvdata(datamval);
					if (MV_FORCE_INT(datamval))
						break;
					if (TREF(want_empty_gvts))
 					{	/* For effective reorg truncates, we need to move empty data blocks in reorg,
						 * so it sets want_empty_gvts before calling gv_select which then calls op_gvorder
						 * Check if any of the spanned regions have a non-zero gv_target->root.
						 * If so, treat it as if op_gvdata is non-zero. That is the only way truncate
						 * can work on this empty GVT.
						 */
						maxi = gvspan->max_reg_index;
						mini = gvspan->min_reg_index;
						for (i = mini; i <= maxi; i++)
						{
							assert(i >= 0);
							assert(i < gd_targ->n_regions);
							gvt = GET_REAL_GVT(gvspan->gvt_array[i - mini]);
							if ((NULL != gvt) && (0 != gvt->root))
								break;
						}
						if (i <= maxi)
							break;	/* found at least one spanned region with non-zero gvt->root */
					}
				} else
				{	/* else gv_target->root would have been initialized by GV_BIND_NAME_AND_ROOT_SEARCH */
					/* For effective truncates , we want to be able to move empty data blocks in reorg.
					 * Reorg truncate calls gv_select which in turn calls op_gvorder.
					 */
					if ((0 != gv_target->root) && (TREF(want_empty_gvts) || (0 != gvcst_data())))
						break;
				}
				GVKEY_INCREMENT_ORDER(gv_currkey);
			}
			if (found)
				break;
			/* do not invoke GVKEY_DECREMENT_ORDER on last map since it contains 0xFFs and will fail an assert */
			if ((map + 1) != map_top)
			{
				assert(strlen(map->gvkey.addr) == map->gvname_len);
				gv_currkey->end = map->gvname_len + 1;
				assert(gv_currkey->end <= (1 + MAX_MIDENT_LEN));
				memcpy(&gv_currkey->base[0], map->gvkey.addr, gv_currkey->end);
				assert(gv_currkey->end < gv_currkey->top);
				gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
				GVKEY_DECREMENT_ORDER(gv_currkey); /* back off 1 spot from map */
			}
		}
		/* Reset gv_currkey as we have potentially skipped one or more regions so we no
		 * longer can expect gv_currkey/gv_cur_region/gv_target to match each other.
		 */
		gv_currkey->end = 0;
		gv_currkey->base[0] = 0;
		v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied by (BYPASSOK)
				* this to-be-overwritten mval */
		if (found)
		{
			if (!IS_STP_SPACE_AVAILABLE(gvname.var_name.len + 1))
			{
				v->str.len = 0;	/* so stp_gcol ignores otherwise incompletely setup mval (BYPASSOK) */
				INVOKE_STP_GCOL(gvname.var_name.len + 1);
			}
			v->str.addr = (char *)stringpool.free;
			*stringpool.free++ = '^';
			memcpy (stringpool.free, gvname.var_name.addr, gvname.var_name.len);
			stringpool.free += gvname.var_name.len;
			v->str.len = gvname.var_name.len + 1;
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	}
	return;
}
