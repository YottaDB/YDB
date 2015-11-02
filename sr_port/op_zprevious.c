/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gvcst_protos.h"	/* for gvcst_data,gvcst_zprevious prototype */
#include "change_reg.h"
#include "gvsub2str.h"
#include "gvcmx.h"
#include "gvusr.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF spdesc		stringpool;

/* op_gvorder should generally be maintained in parallel */

void op_zprevious(mval *v)
{
	int4			n;
	gd_binding		*map;
	mstr			name;
	enum db_acc_method	acc_meth;
	boolean_t		found, ok_to_change_currkey;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gv_currkey->prev || !TREF(gv_last_subsc_null));
	if (gv_currkey->prev)
	{	/* If last subscript is a NULL subscript, modify gv_currkey such that a gvcst_search of the resulting gv_currkey
		 * will find the last available subscript. But in case of dba_usr, (the custom implementation of $ZPREVIOUS which
		 * is overloaded for DDP now but could be more in the future) it is better to hand over gv_currkey as it is so
		 * the custom implementation can decide what to do with it.
		 */
		acc_meth = gv_cur_region->dyn.addr->acc_meth;
		ok_to_change_currkey = (dba_usr != acc_meth);
		if (TREF(gv_last_subsc_null) && ok_to_change_currkey)
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
			GVZPREVIOUS_APPEND_MAX_SUBS_KEY(gv_currkey, gv_target);
		}
		if ((dba_bg == acc_meth) || (dba_mm == acc_meth))
		{
			if (!gv_target->root)	/* global does not exist */
				found = FALSE;
			else
				found = gvcst_zprevious();
		} else  if (dba_cm == acc_meth)
			found = gvcmx_zprevious();
		else
			found = gvusr_zprevious();
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
			stringpool.free = gvsub2str(&gv_altkey->base[gv_altkey->prev], stringpool.free, FALSE);
			v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		if (TREF(gv_last_subsc_null) && ok_to_change_currkey)
		{	/* Restore gv_currkey to what it was at function entry time */
			gv_currkey->base[gv_currkey->prev + 1] = KEY_DELIMITER;
			if (gv_cur_region->std_null_coll)
				gv_currkey->base[gv_currkey->prev] = SUBSCRIPT_STDCOL_NULL;
			assert(gv_cur_region->std_null_coll || (STR_SUB_PREFIX == gv_currkey->base[gv_currkey->prev]));
			gv_currkey->end = gv_currkey->prev + 2;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
		}
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
	} else
	{	/* the following section is for $ZPREVIOUS(^gname) */
		assert(2 <= gv_currkey->end);
		assert(gv_currkey->end < (MAX_MIDENT_LEN + 3));	/* until names are not in midents */
		for (map = gd_map_top - 1; (map > (gd_map + 1)) &&
			(0 >= memcmp(gv_currkey->base, map->name,
				((MAX_MIDENT_LEN + 2) == gv_currkey->end) ? MAX_MIDENT_LEN : gv_currkey->end - 1)); map--)
			;
		for (map++;  map > gd_map;  map--)
		{
			gv_cur_region = map->reg.addr;
			if (!gv_cur_region->open)
				gv_init_reg(gv_cur_region);
			change_reg();
			acc_meth = gv_cur_region->dyn.addr->acc_meth;
			for (;  ;)		/* search region, entries in directory tree could be empty */
			{
				if ((dba_bg == acc_meth) || (dba_mm == acc_meth))
				{
					gv_target = cs_addrs->dir_tree;
					found = gvcst_zprevious();
				} else  if (dba_cm == acc_meth)
					found = gvcmx_zprevious();
				else
					found = gvusr_zprevious();
				if ('#' == gv_altkey->base[0]) /* don't want to give any hidden ^#* global, e.g "^#t" */
					found = FALSE;
				if (!found)
					break;
				assert(1 < gv_altkey->end);
				assert(gv_altkey->end < (MAX_MIDENT_LEN + 2));	/* until names are not in midents */
				if (memcmp(gv_altkey->base, (map - 1)->name, gv_altkey->end) < 0)
				{
					found = FALSE;
					break;
				}
				name.addr = (char *)gv_altkey->base;
				name.len = gv_altkey->end - 1;
				if (dba_cm == acc_meth)
					break;
				GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &name);
				if (gv_cur_region != map->reg.addr)
				{
					found = FALSE;
					break;
				}
				if ((gv_target->root) && gvcst_data())
					break;
			}
			if (found)
				break;
			else  if ((map - 1) > gd_map)
			{
				assert(SIZEOF((map - 1)->name) == SIZEOF(mident_fixed));
				assert(0 == (map - 1)->name[SIZEOF((map - 1)->name) - 1]);
				gv_currkey->end = mid_len((mident_fixed *)((map - 1)->name));
				assert(gv_currkey->end <= MAX_MIDENT_LEN);
				memcpy(gv_currkey->base, (map - 1)->name, gv_currkey->end);
				gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
				gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
				assert(gv_currkey->top > gv_currkey->end);	/* ensure we are within allocated bounds */
			}
		}
		gv_currkey->end = 0;
		gv_currkey->base[0] = KEY_DELIMITER;
		v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied (BYPASSOK)
				* by this to-be-overwritten mval */
		if (found)
		{
			v->str.len = 0; /* so stp_gcol (if invoked) can free up space currently occupied by this (BYPASSOK)
					 * to-be-overwritten mval */
			ENSURE_STP_FREE_SPACE(name.len + 1);
			v->str.addr = (char *)stringpool.free;
			*stringpool.free++ = '^';
			memcpy(stringpool.free, name.addr, name.len);
			stringpool.free += name.len;
			v->str.len = name.len + 1;
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
		v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
		/* No need to restore gv_currkey (to what it was at function entry) as it is already set to NULL */
	}
	return;
}
