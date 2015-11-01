/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "mlkdef.h"
#include "zshow.h"
#include "collseq.h"
#include "stringpool.h"
#include "op.h"
#include "outofband.h"
#include "do_xform.h"
#include "numcmp.h"
#include "patcode.h"
#include "mvalconv.h"
#include "follow.h"
#include "gtm_string.h"

#define eb_less(u, v)    (numcmp(u, v) < 0)

GBLREF lvzwrite_struct 	lvzwrite_block;
GBLREF int4		outofband;
GBLREF zshow_out	*zwr_output;
GBLREF collseq  	*local_collseq;

void lvzwr_var(lv_val *lv, int4 n)
{
	int             length;
	mval 		tmp_sbs;
	mval		mv;
	lv_sbs_tbl	*tbl;
	sbs_blk		*blk;
	sbs_flt_struct	*f;
	sbs_str_struct	*s;
	lv_val		*var;
	char		*top;
	int4		i;
	bool		do_lev;
	zwr_sub_lst	*zwr_sub;
	error_def	(ERR_UNDEF);

	if (lv == zwr_output->out_var.lv.child)
		return;
	if (outofband)
	{
		lvzwrite_block.curr_subsc = lvzwrite_block.subsc_count = 0;
		outofband_action(FALSE);
	}
	lvzwrite_block.curr_subsc = n;
	zwr_sub = (zwr_sub_lst *)lvzwrite_block.sub;
	zwr_sub->subsc_list[n].actual = (mval *)0;
	if ((0 == lvzwrite_block.subsc_count) && (0 == n))
		zwr_sub->subsc_list[n].subsc_type = ZWRITE_ASTERISK;
	if (MV_DEFINED(&(lv->v)) &&
		(!lvzwrite_block.subsc_count || (n == 0 && zwr_sub->subsc_list[n].subsc_type == ZWRITE_ASTERISK) ||
		(n && !(lvzwrite_block.mask >> n))))
			lvzwr_out((mval *)lv);
	if (lvzwrite_block.subsc_count && (n >= lvzwrite_block.subsc_count)
		&& (ZWRITE_ASTERISK != zwr_sub->subsc_list[lvzwrite_block.subsc_count - 1].subsc_type))
		return;

	if (n < lvzwrite_block.subsc_count && zwr_sub->subsc_list[n].subsc_type == ZWRITE_VAL)
	{
		var = op_srchindx(VARLSTCNT(2) lv, zwr_sub->subsc_list[n].first);
		zwr_sub->subsc_list[n].actual = zwr_sub->subsc_list[n].first;
		if (var && (MV_DEFINED(&(var->v)) || n < lvzwrite_block.subsc_count -1))
		{
			lvzwr_var(var, n+1);
			zwr_sub->subsc_list[n].actual = (mval *)0;
			lvzwrite_block.curr_subsc = n;
		} else
		{
			if (lvzwrite_block.fixed)
			{
				unsigned char buff[512], *end;

				lvzwrite_block.curr_subsc++;
				end = lvzwr_key(buff, sizeof(buff));
				zwr_sub->subsc_list[n].actual = (mval *)0;
				lvzwrite_block.curr_subsc = lvzwrite_block.subsc_count = 0;
				rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
			}
		}
	} else  if (tbl = lv->ptrs.val_ent.children)
	{
		zwr_sub->subsc_list[n].actual = &mv;
		mv.mvtype = 0; /* Make sure stp_gcol() ignores zwr_sub->subsc_list[n].actual for now */
		if (tbl->int_flag)
		{
			blk = tbl->num;
			for (i = 0; i < SBS_NUM_INT_ELE; i++)
			{
				if (blk->ptr.lv[i])
				{
					MV_FORCE_MVAL(&mv, i);
					do_lev = TRUE;
					if (n < lvzwrite_block.subsc_count)
					{
						if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)
						{
							if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))
								do_lev = FALSE;
						} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)
						{
							if (zwr_sub->subsc_list[n].first)
							{
							 	if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first) ||
									eb_less(&mv, zwr_sub->subsc_list[n].first))
									do_lev = FALSE;
							}
							if (do_lev && zwr_sub->subsc_list[n].second)
							{
								if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second) &&
									eb_less(zwr_sub->subsc_list[n].second, &mv))
									do_lev = FALSE;
							}
						}
					}

					if (do_lev)
						lvzwr_var(blk->ptr.lv[i], n + 1);
				}
			}
		} else
		{
			for (blk = tbl->num; blk; blk = blk->nxt)
			{
				for (f = &blk->ptr.sbs_flt[0], top = (char*)(f + blk->cnt); f < (sbs_flt_struct *)top; f++)
				{
					MV_ASGN_FLT2MVAL(mv, f->flt);
					do_lev = TRUE;
					if (n < lvzwrite_block.subsc_count)
					{
						if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)
						{
							if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))
								do_lev = FALSE;
						} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)
						{
							if (zwr_sub->subsc_list[n].first)
							{
							 	if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first) ||
									eb_less(&mv, zwr_sub->subsc_list[n].first))
									do_lev = FALSE;
							}
							if (do_lev && zwr_sub->subsc_list[n].second)
							{
								if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second) &&
									eb_less(zwr_sub->subsc_list[n].second, &mv))
									do_lev = FALSE;
							}
						}
					}
					if (do_lev)
						lvzwr_var(f->lv, n + 1);
				}
			}
		}
		mv.mvtype = MV_STR;
		for (blk = tbl->str; blk; blk = blk->nxt)
		{
			for (s = &blk->ptr.sbs_str[0], top = (char *)(s + blk->cnt); s < (sbs_str_struct *)top; s++)
			{
				mv.mvtype = MV_STR;
				mv.str.len = 0; /* makes sure stp_gcol() ignores this, in case actual points to this */
				if (local_collseq)
				{
					ALLOC_XFORM_BUFF(&s->str);
					tmp_sbs.mvtype = MV_STR;
					tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					do_xform(local_collseq, XBACK, &s->str, &tmp_sbs.str, &length);
					tmp_sbs.str.len = length;
					s2pool(&(tmp_sbs.str));
					mv.str = tmp_sbs.str;
				} else
					mv.str = s->str;
				do_lev = TRUE;
				if (n < lvzwrite_block.subsc_count)
				{
					if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)
					{
						if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))
							do_lev = FALSE;
					} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)
					{
						if (zwr_sub->subsc_list[n].first)
						{
							if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first) &&
								(!follow(&mv, zwr_sub->subsc_list[n].first) &&
								(mv.str.len != zwr_sub->subsc_list[n].first->str.len ||
								  memcmp(mv.str.addr, zwr_sub->subsc_list[n].first->str.addr,
									mv.str.len))))
							do_lev = FALSE;
						}
						if (do_lev && zwr_sub->subsc_list[n].second)
						{
							if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second) ||
								(!follow(zwr_sub->subsc_list[n].second, &mv) &&
								(mv.str.len != zwr_sub->subsc_list[n].second->str.len ||
								  memcmp(mv.str.addr,
									zwr_sub->subsc_list[n].second->str.addr,
									mv.str.len))))
							do_lev = FALSE;
						}
					}
				}
				if (do_lev)
					lvzwr_var(s->lv, n + 1);
			}
		}
		zwr_sub->subsc_list[n].actual = (mval *)0;
		lvzwrite_block.curr_subsc = n;
	}
}
