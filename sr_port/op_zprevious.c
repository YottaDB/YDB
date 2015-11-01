/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "stringpool.h"
#include "op.h"
#include "gvcst_data.h"
#include "gvcst_zprevious.h"
#include "change_reg.h"
#include "gvsub2str.h"
#include "gvcmx.h"
#include "gvusr.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF bool		gv_curr_subsc_null;
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
	bool			found;

	if (gv_curr_subsc_null)
	{
		gv_currkey->base[gv_currkey->prev + 1] = 0xFF;
		gv_currkey->base[++(gv_currkey->end)] = 0;
	}
	if (gv_currkey->prev)
	{
		acc_meth = gv_cur_region->dyn.addr->acc_meth;
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
		v->mvtype = MV_STR;
		if (found)
		{
			gv_altkey->prev = gv_currkey->prev;
			if (stringpool.top - stringpool.free < MAX_KEY_SZ)
			{
				if (0xFF != gv_altkey->base[gv_altkey->prev])
					n = MAX_FORM_NUM_SUBLEN;
				else
				{
					n = gv_altkey->end - gv_altkey->prev;
					assert(n > 0);
				}
				if (stringpool.top - stringpool.free < n)
					stp_gcol(n);
			}
			v->str.addr = (char *)stringpool.free;
			stringpool.free = gvsub2str(&gv_altkey->base[gv_altkey->prev], stringpool.free, FALSE);
			v->str.len = (char *)stringpool.free - v->str.addr;
			assert(v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert(v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		} else
			v->str.len = 0;
	} else
	{	/* the following section is for $ZPREVIOUS(^gname) */
		assert(2 <= gv_currkey->end);
		assert(gv_currkey->end < (sizeof(mident) + 3));	/* until names are not in midents */
		assert(INVALID_GV_TARGET == reset_gv_target);
		reset_gv_target = gv_target;
		for (map = gd_map_top - 1;  (map > (gd_map + 1)) &&
			(0 >= memcmp(gv_currkey->base, map->name,
				((sizeof(mident) + 2) == gv_currkey->end) ? sizeof(mident) : gv_currkey->end - 1));  map--)
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
				if (!found)
					break;
				assert(1 < gv_altkey->end);
				assert(gv_altkey->end < (sizeof(mident) + 2));	/* until names are not in midents */
				if (memcmp(gv_altkey->base, (map - 1)->name, gv_altkey->end - 1) < 0)
				{
					found = FALSE;
					break;
				}
				name.addr = (char *)gv_altkey->base;
				name.len = gv_altkey->end - 1;
				if (dba_cm == acc_meth)
					break;
				gv_bind_name(gd_header, &name);
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
				memcpy(gv_currkey->base, (map - 1)->name, sizeof(mident));
				gv_currkey->end = mid_len((mident *)gv_currkey->base);
				gv_currkey->base[gv_currkey->end++] = 0;
				gv_currkey->base[gv_currkey->end++] = 0;
			}
		}
		gv_currkey->end = 0;
		gv_currkey->base[0] = 0;
		RESET_GV_TARGET;
		v->mvtype = MV_STR;
		if (found)
		{
			if (stringpool.free + name.len + 1 > stringpool.top)
				stp_gcol(name.len + 1);
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
	}
	return;
}
