/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "zwrite.h"
#include "op.h"
#include "deferred_events_queue.h"
#include "numcmp.h"
#include "patcode.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "zwr_follow.h"
#include "gtm_string.h"
#include "gtmimagename.h"
#include "numcmp.h"

GBLREF bool		undef_inhibit;
GBLREF gd_addr		*gd_header;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF gvzwrite_datablk *gvzwrite_block;
GBLREF volatile int4   outofband;

LITREF mval		literal_null;

void gvzwr_var(uint4 data, int4 n)
{
	mval		mv, subdata;
	unsigned short  end, prev, end1, prev1;
	bool		save_gv_last_subsc_null;
	boolean_t	do_lev;
	char		seen_null;
	zwr_sub_lst	*zwr_sub;
	int		loop_condition = 1;
	gvnh_reg_t	*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (outofband)
		async_action(FALSE);
	zwr_sub = (zwr_sub_lst *)gvzwrite_block->sub;
	if ((0 == gvzwrite_block->subsc_count) && (0 == n))
		zwr_sub->subsc_list[n].subsc_type = ZWRITE_ASTERISK;
	if ((1 == data || 11 == data) &&
	    (!gvzwrite_block->subsc_count || (ZWRITE_ASTERISK == zwr_sub->subsc_list[n].subsc_type) ||
	     (n && !(gvzwrite_block->mask >> n))))
		gvzwr_out();
	if ((1 >= data) || (gvzwrite_block->subsc_count && (n >= gvzwrite_block->subsc_count)
			    && (ZWRITE_ASTERISK != zwr_sub->subsc_list[gvzwrite_block->subsc_count - 1].subsc_type)))
		return;
	assert(1 < data);
	end = gv_currkey->end;
	prev = gv_currkey->prev;
	gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set by op_gvname call done in gvzwr_fini before call to gvzwr_var */
	if ((n < gvzwrite_block->subsc_count) && (ZWRITE_VAL == zwr_sub->subsc_list[n].subsc_type))
	{
		mval2subsc(zwr_sub->subsc_list[n].first, gv_currkey, gv_cur_region->std_null_coll);
		/* If gvnh_reg corresponds to a spanning global, then determine
		 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
		 */
		GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey);
		op_gvdata(&subdata);
		if (MV_FORCE_INTD(&subdata) && ((10 != (int4)MV_FORCE_INTD(&subdata)) || n < gvzwrite_block->subsc_count - 1))
		{
			save_gv_last_subsc_null = TREF(gv_last_subsc_null);
			gvzwr_var((int4)MV_FORCE_INTD(&subdata), n + 1);
			TREF(gv_last_subsc_null) = save_gv_last_subsc_null;
		} else if (gvzwrite_block->fixed && (!undef_inhibit))
			sgnl_gvundef();
	} else
	{
		seen_null = 0;
		if (n < gvzwrite_block->subsc_count
		    && zwr_sub->subsc_list[n].first
		    && ZWRITE_PATTERN != zwr_sub->subsc_list[n].subsc_type)
		{
			mv = *zwr_sub->subsc_list[n].first;
			mval2subsc(&mv, gv_currkey, gv_cur_region->std_null_coll);
			if ((mv.mvtype & MV_STR) && !mv.str.len)
				seen_null = 1;
			/* If gvnh_reg corresponds to a spanning global, then determine
			 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
			 */
			GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey);
			op_gvdata(&subdata);
		} else
		{
			mval2subsc((mval *)&literal_null, gv_currkey, gv_cur_region->std_null_coll);
			TREF(gv_last_subsc_null) = TRUE;
			if (0 == gv_cur_region->std_null_coll)
			{
				op_gvorder(&mv); /* This will return the first subscript */
				if (0 == mv.str.len)
				{
					if (NEVER == gv_cur_region->null_subs || seen_null)
						loop_condition = 0;
					else
					{
						seen_null = 1;			/* set flag to indicate processing null sub */
						op_gvnaked(VARLSTCNT(1) &mv);
						op_gvdata(&subdata);
						if (!MV_FORCE_INTD(&subdata))
							loop_condition = 0;
					}
				} else
				{
					op_gvnaked(VARLSTCNT(1) &mv);
					op_gvdata(&subdata);
				}
			} else /* for standard null collation */
			{	/* determine whether $data(^gbl("") == 1 or 11, if yes, first process that */
				if (NEVER == gv_cur_region->null_subs)
				{
					op_gvorder(&mv);
					assert(0 != mv.str.len); /* We are looking for the first subscript at a given level and so,
								    we do not expect to have hit at the end of the list */
					op_gvnaked(VARLSTCNT(1) &mv);
					op_gvdata(&subdata);
				} else
				{
					/* If gvnh_reg corresponds to a spanning global, then determine
					 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
					 */
					GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey);
					mv = literal_null;
					op_gvdata(&subdata);
					if (MV_FORCE_INTD(&subdata))
						seen_null = 1;
				}
			}
		}
		while (loop_condition)
		{
			do_lev = (MV_FORCE_INTD(&subdata) ? TRUE : FALSE);
			if (n < gvzwrite_block->subsc_count)
			{
				if (ZWRITE_PATTERN == zwr_sub->subsc_list[n].subsc_type)
				{
					if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))
						do_lev = FALSE;
				} else if (ZWRITE_ALL != zwr_sub->subsc_list[n].subsc_type)
				{
					if (do_lev && zwr_sub->subsc_list[n].first)
					{
						if (MV_IS_CANONICAL(&mv))
						{
							if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first)
									|| (0 > numcmp(&mv, zwr_sub->subsc_list[n].first)))
								do_lev = FALSE;
						} else
						{
							if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first)
							    && (!zwr_follow(&mv, zwr_sub->subsc_list[n].first) &&
								(mv.str.len != zwr_sub->subsc_list[n].first->str.len ||
								 memcmp(mv.str.addr,
									zwr_sub->subsc_list[n].first->str.addr,
									mv.str.len))))
								do_lev = FALSE;
						}
					}

					if (do_lev && zwr_sub->subsc_list[n].second)
					{
						if (MV_IS_CANONICAL(&mv))
						{
							if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second)
									&& (0 > numcmp(zwr_sub->subsc_list[n].second, &mv)))
								do_lev = FALSE;
						} else
						{
							if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second)
							    ||	(!zwr_follow(zwr_sub->subsc_list[n].second, &mv) &&
								 (mv.str.len != zwr_sub->subsc_list[n].second->str.len ||
								  memcmp(mv.str.addr,
									 zwr_sub->subsc_list[n].second->str.addr,
									 mv.str.len))))
								do_lev = FALSE;
						}
						if (!do_lev)
							break;
					}
				}
			}
			if (do_lev)
			{
				end1 = gv_currkey->end;
				prev1 = gv_currkey->prev;
				save_gv_last_subsc_null = TREF(gv_last_subsc_null);
				gvzwr_var((int4)MV_FORCE_INTD(&subdata), n + 1);
				TREF(gv_last_subsc_null) = save_gv_last_subsc_null;
				gv_currkey->end = end1;
				gv_currkey->prev = prev1;
				gv_currkey->base[end1] = 0;
			}
			if (1 == seen_null)
			{
				assert(TREF(gv_last_subsc_null));
				TREF(gv_last_subsc_null) = FALSE;
				seen_null = 2;				/* set flag to indicate null sub processed */
			}
			op_gvorder(&mv);
			/* When null subscript is in the middle,
			   but with standard collation null subscripts can not be in the middle, so don't need to be worried
			*/
			if (0 == mv.str.len)
			{
				if (NEVER == gv_cur_region->null_subs || seen_null || gv_cur_region->std_null_coll)
					break;
				else
				{
					seen_null = 1;			/* set flag to indicate processing null sub */
					op_gvnaked(VARLSTCNT(1) &mv);
					op_gvdata(&subdata);
					if (!MV_FORCE_INTD(&subdata))
						break;
				}
			} else
			{
				op_gvnaked(VARLSTCNT(1) &mv);
				op_gvdata(&subdata);
			}
		}
	}
	gv_currkey->end = end;
	gv_currkey->prev = prev;
	gv_currkey->base[end] = 0;
}
