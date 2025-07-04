/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>			/* for offsetof macro in VMS */

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"		/* for gvcst_data,gvcst_zprevious prototype */
#include "change_reg.h"
#include "gvsub2str.h"
#include "gvcmx.h"
#include "filestruct.h"
#include "hashtab_mname.h"
#include "targ_alloc.h"			/* for GV_BIND_SUBSREG macro which needs "targ_alloc" prototype */
#include "gtmimagename.h"
#include "collseq.h"			/* for STD_NULL_COLL_FALSE */
#include "mvalconv.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */
#include "gvt_inline.h"

GBLREF gd_region		*gv_cur_region;
GBLREF gv_namehead		*gv_target;
GBLREF gv_key			*gv_altkey, *gv_currkey;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF sgm_info			*sgm_info_ptr;
GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF spdesc			stringpool;

/* op_gvorder should generally be maintained in parallel */

void op_zprevious(mval *v)
{
	boolean_t		found;
	enum db_acc_method	acc_meth;
	gd_addr			*gd_targ;
	gd_binding		*gd_map_start, *map, *prev_map, *rmap;
	gd_region		*save_gv_cur_region, *reg;
	gv_key_buf		save_currkey;
	gv_namehead		*save_gv_target;
	gvnh_reg_t		*gvnh_reg;
	int			min_reg_index, reg_index, res;
	int4			n;
	mname_entry		gvname;
	mval			tmpmval, *datamval;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gv_currkey->prev || !TREF(gv_last_subsc_null));
	if (gv_currkey->prev)
	{	/* If last subscript is a NULL subscript, modify gv_currkey such that a gvcst_search of the resulting gv_currkey
		 * will find the last available subscript.
		 */
		acc_meth = REG_ACC_METH(gv_cur_region);
		assert(dba_usr != acc_meth);
		if (TREF(gv_last_subsc_null))
		{	/* Replace the last subscript with the highest possible subscript value i.e. the byte sequence
			 * 	0xFF (STR_SUB_MAXVAL), 0xFF, 0xFF ...  as much as possible i.e. until gv_currkey->top permits.
			 * This subscript is guaranteed to be NOT present in the database since a user who tried to set this
			 * exact subscripted global would have gotten a GVSUBOFLOW error (because GT.M sets aside a few bytes
			 * of padding space). And yet this is guaranteed to collate AFTER any existing subscript. Therefore we
			 * can safely do a gvcst_zprevious on this key to get at the last existing key in the database.
			 *
			 * With    standard null collation, the last subscript will be 0x01
			 * Without standard null collation, the last subscript will be 0xFF
			 * Assert that is indeed the case as this will be used to restore the replaced subscript at the end.
			 */
			assert(gv_cur_region->std_null_coll || (STR_SUB_PREFIX == gv_currkey->base[gv_currkey->prev]));
			assert(!gv_cur_region->std_null_coll || (SUBSCRIPT_STDCOL_NULL == gv_currkey->base[gv_currkey->prev]));
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->prev + 1]);
			assert(gv_currkey->end == gv_currkey->prev + 2);
			assert(gv_currkey->end < gv_currkey->top); /* need "<" (not "<=") to account for terminating 0x00 */
			gv_append_max_subs_key(gv_currkey, gv_target);
		}
		if (IS_ACC_METH_BG_OR_MM(acc_meth))
		{
			gvnh_reg = TREF(gd_targ_gvnh_reg);
			if (NULL == gvnh_reg)
				found = (gv_target->root ? gvcst_zprevious() : FALSE);
			else
				INVOKE_GVCST_SPR_XXX(gvnh_reg, found = gvcst_spr_zprevious());
		} else
		{
			assert(acc_meth == dba_cm);
			found = gvcmx_zprevious();
		}
		v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied (BYPASSOK)
				* by this to-be-overwritten mval */
		if (found)
		{
			gv_altkey->prev = gv_currkey->prev;
			if (!IS_STP_SPACE_AVAILABLE(MAX_KEY_SZ))
			{
				if ((0xFF != gv_altkey->base[gv_altkey->prev])
						&& (SUBSCRIPT_STDCOL_NULL != gv_altkey->base[gv_altkey->prev]))
					n = MAX_FORM_NUM_SUBLEN;
				else
				{
					n = gv_altkey->end - gv_altkey->prev;
					assert(n > 0);
				}
				v->str.len = 0; /* so stp_gcol (if invoked) can free up space currently occupied by this (BYPASSOK)
						 * to-be-overwritten mval */
				ENSURE_STP_FREE_SPACE(n);
			}
			v->str.addr = (char *)stringpool.free;
			v->str.len = MAX_KEY_SZ;
			stringpool.free = gvsub2str(&gv_altkey->base[gv_altkey->prev], &(v->str), FALSE);
			v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		if (TREF(gv_last_subsc_null))
		{	/* Restore gv_currkey to what it was at function entry time */
			gv_undo_append_max_subs_key(gv_currkey, gv_cur_region);
		}
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
	} else
	{	/* the following section is for $ZPREVIOUS(^gname) : name-level $zprevious */
		assert(2 <= gv_currkey->end);
		assert(gv_currkey->end < (MAX_MIDENT_LEN + 2));	/* until names are not in midents */
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
		SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
		gd_targ = TREF(gd_targ_addr);
		gd_map_start = gd_targ->maps;
		map = gv_srch_map(gd_targ, (char *)&gv_currkey->base[0], gv_currkey->end - 1, SKIP_BASEDB_OPEN_FALSE);
		assert(map > (gd_map_start + 1));
		/* If ^gname starts at "map" start search from map-1 since $ZPREVIOUS(^gname) is sought */
		BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gv_currkey->base, gv_currkey->end - 1, map);
		found = FALSE;
		/* The first map entry corresponds to local locks. The second map entry does not contain any globals.
		 * Therefore, any search for globals needs to only look after these maps. Hence the "gd_map_start + 1" below.
		 */
		for ( ; map > gd_map_start + 1; map = prev_map)
		{
			prev_map = map - 1;
			reg = map->reg.addr;
			/* If region corresponding to the map is a statsDB region (lowercase region name) then it could contain
			 * special globals ^%Y* (e.g. ^%YGS) which we don't want to be visible in name-level $zprevious.
			 * So skip the region altogether.
			 */
			if (IS_BASEDB_REGNAME(reg))
			{	/* Non-statsDB region */
				if (!reg->open)
					gv_init_reg(reg);
				/* Entries in directory tree could have empty GVT in which case move on to previous entry */
				CHANGE_REG_IF_NEEDED(map->reg.addr);
				acc_meth = REG_ACC_METH(gv_cur_region);
				for ( ; ; )
				{
					assert(gv_cur_region == map->reg.addr);
					assert(0 == gv_currkey->prev);	/* or else gvcst_zprevious could get confused */
					if (IS_ACC_METH_BG_OR_MM(acc_meth))
					{
						gv_target = cs_addrs->dir_tree;
						found = gvcst_zprevious();
					} else
					{
						assert(acc_meth == dba_cm);
						found = gvcmx_zprevious();
					}
					if ('#' == gv_altkey->base[0]) /* don't want to give any hidden ^#* global, e.g "^#t" */
						found = FALSE;
					if (!found)
						break;
					assert(1 < gv_altkey->end);
					assert(gv_altkey->end < (MAX_MIDENT_LEN + 2));	/* until names are not in midents */
					res = memcmp(gv_altkey->base, prev_map->gvkey.addr, gv_altkey->end);
					assert((0 != res) || (gv_altkey->end <= prev_map->gvkey_len));
					if (0 > res)
					{	/* The global name we found is less than the maximum value in the previous map
						 * so this name is not part of the current map for sure. Move on to previous map.
						 */
						found = FALSE;
						break;
					}
					gvname.var_name.addr = (char *)gv_altkey->base;
					gvname.var_name.len = gv_altkey->end - 1;
					if (dba_cm == acc_meth)
						break;
					COMPUTE_HASH_MNAME(&gvname);
					GV_BIND_NAME_AND_ROOT_SEARCH(gd_targ, &gvname, gvnh_reg);	/* updates "gv_currkey" */
					assert((NULL != gvnh_reg->gvspan) || (gv_cur_region == map->reg.addr));
					if (NULL != gvnh_reg->gvspan)
					{	/* For accurate results, we need to see if the node found by gvcst_zprevious()
						 * has data or that its first data child lies in the same map. Otherwise,
						 * the node should be excluded.
						 * To do this, take the proposed key and do a gvcst_query2() on it.
						 * If the resulting key matches the request and is in the current map,
						 * use it to derive the result. Otherwise, put things back to where they
						 * were after the search and continue.
						 */
						if ((prev_map->gvkey_len > gv_currkey->end)
							&& (0 == memcmp(gv_currkey->base, prev_map->gvkey.addr, gv_currkey->end)))
						{	/* We found the same global name as the previous map's endpoint,
							 * so search forward from there to make sure we found something
							 * in this map by borrowing the endpoint's subscript(s).
							 */
							memcpy(gv_currkey->base + gv_currkey->end,
									prev_map->gvkey.addr + gv_currkey->end,
									prev_map->gvkey_len - gv_currkey->end);
							gv_currkey->end = prev_map->gvkey_len;
							gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
						}
						GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_targ, gv_currkey, gvnh_reg->gd_reg);
						found = gv_target->root && gvcst_query2();
						if (found && (gv_altkey->end >= gvname.var_name.len + 1)
							&& (0 == memcmp(gv_currkey->base, gv_altkey->base,
										gvname.var_name.len + 1)))
						{
							rmap = gv_srch_map_linear_backward(map, (char *)gv_altkey->base,
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
						/* We may have added subscripts from the previous map above, so restore
						 * gv_currkey to the name alone by using the gvname we saved above.
						 */
						gv_currkey->end = gvname.var_name.len + 1;
						gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
					} else
					{	/* else gv_target->root would have been initialized by
						 * GV_BIND_NAME_AND_ROOT_SEARCH
						 */
						if ((0 != gv_target->root) && (0 != gvcst_data()))
							break;
					}
				}
				if (found)
					break;
			} else
				found = FALSE;	/* Skip statsDB region completely. So set found to FALSE */
			/* At this point, gv_currkey may be one of:
			 * -- The initial gv_currkey, or one from a previous iteration. (map to statsdb region)
			 * -- The result of gvcst_zprevious(), possibly mangled/restored (gvspan + prev_map name match case)
			 */
			assert(prev_map->gvkey_len >= (prev_map->gvname_len + 1));
			if (prev_map > (gd_map_start + 1))
			{	/* Base the continued search on the previous map endpoint. */
				assert(strlen(prev_map->gvkey.addr) == prev_map->gvname_len);
				gv_currkey->end = prev_map->gvname_len + 1;
				assert(gv_currkey->end <= (MAX_MIDENT_LEN + 1));
				memcpy(gv_currkey->base, prev_map->gvkey.addr, gv_currkey->end);
				assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
				gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
				assert(gv_currkey->top > gv_currkey->end);	/* ensure we are within allocated bounds */
				if (prev_map->gvkey_len != (prev_map->gvname_len + 1))
				{	/* The prior map endpoint has subscripts, and we may want to match its name, so start the
					 * search after the name.
					 */
					GVKEY_INCREMENT_ORDER(gv_currkey);
				}
			}
		}
		v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied (BYPASSOK)
				* by this to-be-overwritten mval */
		if (found)
		{
			if (!IS_STP_SPACE_AVAILABLE(gvname.var_name.len + 1))
			{
				v->str.len = 0;	/* so stp_gcol ignores otherwise incompletely setup mval (BYPASSOK) */
				INVOKE_STP_GCOL(gvname.var_name.len + 1);
			}
			v->str.addr = (char *)stringpool.free;
			*stringpool.free++ = '^';
			memcpy(stringpool.free, gvname.var_name.addr, gvname.var_name.len);
			stringpool.free += gvname.var_name.len;
			v->str.len = gvname.var_name.len + 1;
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		/* No need to restore gv_currkey (to what it was at function entry) as it is already set to NULL */
		RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	}
	return;
}
