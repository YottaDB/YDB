/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "hashtab_mname.h"
#include "targ_alloc.h"		/* for GV_BIND_SUBSREG macro which needs "targ_alloc" prototype */
#include "gtmimagename.h"
#include "mvalconv.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */
#include "gvt_inline.h"


GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_altkey, *gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF sgm_info			*sgm_info_ptr;
GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF spdesc			stringpool;

/* op_zprevious should generally be maintained in parallel */

void op_gvorder(mval *v)
{
	boolean_t		found;
	enum db_acc_method	acc_meth;
	gd_addr			*gd_targ;
	gd_binding		*gd_map_start, *map, *map_top, *start_map, *rmap;
	gd_region		*save_gv_cur_region, *reg, *subreg;
	gv_key_buf		save_currkey;
	gv_namehead		*gvt, *save_gv_target;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	int			i, maxi, min_reg_index, mini, reg_index, res;
	int4			n;
	mname_entry		gvname;
	mstr			opstr;
	mval			tmpmval, *datamval;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	acc_meth = REG_ACC_METH(gv_cur_region);
	/* Modify gv_currkey such that a gvcst_search of the resulting gv_currkey will find the next available subscript. */
	/* Modify gv_currkey to reflect the next possible key value in collating order */
	assert(dba_usr != acc_meth);
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
	if (gv_currkey->prev)
	{
		if (IS_ACC_METH_BG_OR_MM(acc_meth))
		{
			gvnh_reg = TREF(gd_targ_gvnh_reg);
			if (NULL == gvnh_reg)
				found = (gv_target->root ? gvcst_order() : FALSE);
			else
				INVOKE_GVCST_SPR_XXX(gvnh_reg, found = gvcst_spr_order());
		} else
		{
			assert(acc_meth == dba_cm);
			found = gvcmx_order();
		}
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
			opstr.addr = v->str.addr;
			opstr.len = MAX_ZWR_KEY_SZ;
			stringpool.free = gvsub2str(&gv_altkey->base[0] + gv_altkey->prev, &opstr, FALSE);
			v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		/* Restore gv_currkey to what it was at function entry time */
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
	} else	/* the following section is for $O(^gname) : name-level $order */
	{
		assert(2 < gv_currkey->end);
		assert(gv_currkey->end < (MAX_MIDENT_LEN + 3));	/* until names are not in midents */
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
		SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
		if (STR_SUB_ESCAPE == *(save_currkey.split.base + gv_currkey->end - 2))
		{	/* strip the byte added to get past curr */
			*(save_currkey.split.base + gv_currkey->end - 2) = KEY_DELIMITER;
		}
		gd_targ = TREF(gd_targ_addr);
		gd_map_start = gd_targ->maps;
		map_top = gd_map_start + gd_targ->n_maps;
		map = start_map = gv_srch_map(gd_targ, (char *)&gv_currkey->base[0], gv_currkey->end - 1, SKIP_BASEDB_OPEN_FALSE);
		for ( ; map < map_top; ++map)
		{
			reg = map->reg.addr;
			/* If region corresponding to the map is a statsDB region (lowercase region name) then it could contain
			 * special globals ^%Y* (e.g. ^%YGS) which we don't want to be visible in name-level $order.
			 * So skip the region altogether.
			 */
			if (IS_BASEDB_REGNAME(reg))
			{	/* Non-statsDB region */
				if (!reg->open)
					gv_init_reg(reg);
				/* Entries in directory tree of region could have empty GVT in which case move on to next entry */
				CHANGE_REG_IF_NEEDED(map->reg.addr);
				acc_meth = REG_ACC_METH(gv_cur_region);
				for ( ; ; )
				{
					assert(gv_cur_region == map->reg.addr);
					if (IS_ACC_METH_BG_OR_MM(acc_meth))
					{
						gv_target = cs_addrs->dir_tree;
						found = gvcst_order();
					} else
					{
						assert(acc_meth == dba_cm);
						found = gvcmx_order();
					}
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
					gvname.var_name.addr = (char *)gv_altkey->base;
					gvname.var_name.len = gv_altkey->end - 1;
					if (dba_cm == acc_meth)
						break;
					COMPUTE_HASH_MNAME(&gvname);
					GV_BIND_NAME_AND_ROOT_SEARCH(gd_targ, &gvname, gvnh_reg); /* updates "gv_currkey" */
					GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_targ, gv_currkey, subreg);
					gvspan = gvnh_reg->gvspan;
					assert((NULL != gvspan) || (gv_cur_region == map->reg.addr));
					if (TREF(want_empty_gvts) && (NULL != gvspan))
					{	/* For effective reorg truncates, we need to move empty data blocks
						 * in reorg, so it sets want_empty_gvts before calling gv_select
						 * which then calls op_gvorder. Check if any of the spanned regions
						 * have a non-zero gv_target->root. If so, treat it as if op_gvdata
						 * is non-zero. That is the only way truncate can work on this
						 * empty GVT.
						 */
						datamval = &tmpmval;
						op_gvdata(datamval);
						if (MV_FORCE_INT(datamval))
							break;
						gvspan = gvnh_reg->gvspan;
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
							break;	/* Found spanned region with non-zero gvt->root */
					} else if (NULL == gvspan)
					{	/* else gv_target->root would have been initialized by
						 * GV_BIND_NAME_AND_ROOT_SEARCH.
						 */
						/* For effective truncates , we want to be able to move empty data blocks in reorg.
						 * Reorg truncate calls gv_select which in turn calls op_gvorder.
						 */
						if ((0 != gv_target->root) && (TREF(want_empty_gvts) || (0 != gvcst_data())))
							break;
					} else
					{	/* For accurate results, we need to see if the node found by gvcst_order()
						 * has data or that its first data child lies in the same map. Otherwise,
						 * the node should be excluded.
						 * To do this, take the proposed key and do a gvcst_query2() on it.
						 * If the resulting key matches the request and is in the current map,
						 * use it to derive the result. Otherwise, put things back to where they
						 * were after the search and continue.
						 */
						if ((map != start_map)
							&& (map[-1].gvkey_len + 1 > gv_currkey->end)
							&& (0 == memcmp(gv_currkey->base, map[-1].gvkey.addr,
											gv_currkey->end)))
						{	/* The key we are searching is a prefix of the previous map,
							 * so search from the end of that map to make sure that we
							 * don't match anything earlier.
							 */
							GV_BIND_NAME_AND_ROOT_SEARCH(gd_targ, &gvname, gvnh_reg);
							memcpy(gv_currkey->base + gv_currkey->end,
									map[-1].gvkey.addr + gv_currkey->end,
									map[-1].gvkey_len - gv_currkey->end);
							gv_currkey->end = map[-1].gvkey_len;
							gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
							/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH
							 * 	(e.g. setting gv_cur_region for spanning globals)
							 */
							GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_targ, gv_currkey, subreg);
						}
						found = gv_target->root && gvcst_query2();
						if (found)
						{
							rmap = gv_srch_map_linear(start_map, (char *)gv_altkey->base,
											gv_altkey->end - 1);
							if (map == rmap)
							{
								for (gv_altkey->end = 0;
										gv_altkey->base[gv_altkey->end];
										gv_altkey->end++)
									;
								gv_altkey->base[++gv_altkey->end] = KEY_DELIMITER;
								found = TRUE;
								break;
							}
						}
						/* The query failed, but we may still find something in the next global, so
						 * reset gv_currkey to the global name so the increment below can work
						 * properly.
						 */
						for (gv_currkey->end = 0;
								gv_currkey->base[gv_currkey->end] != KEY_DELIMITER;
								gv_currkey->end++)
							;
						gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
						gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
					}
					GVKEY_INCREMENT_ORDER(gv_currkey);
				}
				if (found)
					break;
			} else
				found = FALSE;	/* Skip statsDB region completely. So set found to FALSE */
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
		RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	}
	return;
}
