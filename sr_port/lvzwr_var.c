/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "hashtab_mname.h"
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
#include "alias.h"

#define eb_less(u, v)    (numcmp(u, v) < 0)

#define COMMON_STR_PROCESSING											\
{														\
	mv.mvtype = MV_STR;											\
	mv.str.len = 0; /* makes sure stp_gcol() ignores this, in case actual points to this */			\
	if (local_collseq)											\
	{													\
		ALLOC_XFORM_BUFF(&s->str);									\
		tmp_sbs.mvtype = MV_STR;									\
		tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;							\
		assert(NULL != lcl_coll_xform_buff);								\
		tmp_sbs.str.addr = lcl_coll_xform_buff;								\
		do_xform(local_collseq, XBACK, &s->str, &tmp_sbs.str, &length);					\
		tmp_sbs.str.len = length;									\
		s2pool(&(tmp_sbs.str));										\
		mv.str = tmp_sbs.str;										\
	} else													\
		mv.str = s->str;										\
	do_lev = TRUE;												\
	if (n < lvzwrite_block->subsc_count)									\
	{													\
		if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)					\
		{												\
			if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))					\
				do_lev = FALSE;									\
		} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)					\
		{												\
			if (zwr_sub->subsc_list[n].first)							\
			{											\
				if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first) &&				\
						(!follow(&mv, zwr_sub->subsc_list[n].first) &&			\
						(mv.str.len != zwr_sub->subsc_list[n].first->str.len ||		\
					  	memcmp(mv.str.addr, zwr_sub->subsc_list[n].first->str.addr,	\
						mv.str.len))))							\
					do_lev = FALSE;								\
			}											\
			if (do_lev && zwr_sub->subsc_list[n].second)						\
			{											\
				if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second) ||				\
						(!follow(zwr_sub->subsc_list[n].second, &mv) &&			\
						(mv.str.len != zwr_sub->subsc_list[n].second->str.len ||	\
					  	memcmp(mv.str.addr,						\
						zwr_sub->subsc_list[n].second->str.addr,			\
						mv.str.len))))							\
					do_lev = FALSE;								\
			}											\
		}												\
	}													\
	if (do_lev)												\
		lvzwr_var(s->lv, n + 1);									\
}

#define COMMON_NUMERIC_PROCESSING										\
{														\
	if (n < lvzwrite_block->subsc_count)									\
	{													\
		if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)					\
		{												\
			if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))					\
				do_lev = FALSE;									\
		} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)					\
		{												\
			if (zwr_sub->subsc_list[n].first)							\
			{											\
		 		if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first) ||				\
						eb_less(&mv, zwr_sub->subsc_list[n].first))			\
					do_lev = FALSE;								\
			}											\
			if (do_lev && zwr_sub->subsc_list[n].second)						\
			{											\
				if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second) &&				\
						eb_less(zwr_sub->subsc_list[n].second, &mv))			\
					do_lev = FALSE;								\
			}											\
		}												\
	}													\
}

GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF int4		outofband;
GBLREF zshow_out	*zwr_output;
GBLREF collseq  	*local_collseq;
GBLREF boolean_t 	local_collseq_stdnull;
GBLREF int		merge_args;
GBLREF zwr_hash_table	*zwrhtab;			/* How we track aliases during zwrites */

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
	boolean_t	do_lev, verify_hash_add, htent_added, value_printed_pending;
	zwr_sub_lst	*zwr_sub;
	ht_ent_addr	*tabent_addr;
	zwr_alias_var	*zav, *newzav;

	error_def	(ERR_UNDEF);

	assert(lvzwrite_block);
	if (lv == zwr_output->out_var.lv.child)
		return;
	if (outofband)
	{
		lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
		outofband_action(FALSE);
	}
	lvzwrite_block->curr_subsc = n;
	zwr_sub = (zwr_sub_lst *)lvzwrite_block->sub;
	zwr_sub->subsc_list[n].actual = (mval *)NULL;

	/* Before we process this var, there are some special cases to check for first when
	   this is a base var (0 == lvzwrite_block->subsc_count) and the var is an alias.

	   - Check if we have seen it before (the lvval is in the zwr_alias_var hash table), then we
	     need to process this var with lvzwr_out NOW and we will only be processing the base
	     var, not any of the subscripts. This is because all those subscripts (and the value
	     of the base var itself) have been dealt with previously when we first saw this
	     lvval. So in that case, call lvzwr_out() to output the association after which we are
	     done with this var.
	  -  If we haven't seen it before, set a flag so we verify if the base var gets processed by
	     lvzwr_out or not (i.e. whether it has a value and the "subscript" or lack there of is
	     either wildcarded or whatever so that it actually gets dumped by lvzwr_out (see conditions
	     below). If not, then *we* need to add the lvval to the hash table to signify we have seen
	     it before so the proper associations to this alias var can be printed at a later time
	     when/if they are encountered.
	*/
	verify_hash_add = FALSE;	/* By default we don't need to verify add */
	value_printed_pending = FALSE;	/* Force the "value_printed" flag on if TRUE */
	zav = NULL;
	if (!merge_args && IS_ALIASLV(lv))
	{
		assert(0 == n);	/* Verify base var lv_val */
		if (tabent_addr = (ht_ent_addr *)lookup_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lv))
		{	/* We've seen it before but check if it was actually printed at that point */
			zav = (zwr_alias_var *)tabent_addr->value;
			assert(zav);
			if (zav->value_printed)
			{
				lvzwr_out(lv);
				lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
				return;
			} else
				value_printed_pending = TRUE;	/* We will set value_printed flag true later */
		} else
			verify_hash_add = TRUE;
	}

	if ((0 == lvzwrite_block->subsc_count) && (0 == n))
		zwr_sub->subsc_list[n].subsc_type = ZWRITE_ASTERISK;
	if (MV_DEFINED(&(lv->v))
	    && (!lvzwrite_block->subsc_count || ((0 == n) && ZWRITE_ASTERISK == zwr_sub->subsc_list[n].subsc_type)
		|| ((0 != n) && !(lvzwrite_block->mask >> n))))
	{
		lvzwr_out(lv);
	}
	if (verify_hash_add && !lvzwrite_block->zav_added)
	{	/* lvzwr_out processing didn't add a zav for this var. Take care of that now so we
		   recognize it as a "dealt with" alias when they are encountered later.
		*/
		newzav = als_getzavslot();
		newzav->ptrs.val_ent.zwr_var = *lvzwrite_block->curr_name;
		newzav->value_printed = TRUE;
		htent_added = add_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lv, newzav, &tabent_addr);
		assert(htent_added);
	}
	/* If we processed a base var above to print an alias association but it hadn't been printed yet,
	   we had to wait until after lvzwr_out() was called before we could set the flag that indicated
	   the printing had occurred. Do that now. Note that it is only when this flag is set we are
	   certain to have a good value in zav.
	*/
	if (value_printed_pending)
	{
		assert(zav);
		zav->value_printed = TRUE;
	}
	if (lvzwrite_block->subsc_count && (n >= lvzwrite_block->subsc_count)
		&& (ZWRITE_ASTERISK != zwr_sub->subsc_list[lvzwrite_block->subsc_count - 1].subsc_type))
		return;

	if (n < lvzwrite_block->subsc_count && ZWRITE_VAL == zwr_sub->subsc_list[n].subsc_type)
	{
		var = op_srchindx(VARLSTCNT(2) lv, zwr_sub->subsc_list[n].first);
		zwr_sub->subsc_list[n].actual = zwr_sub->subsc_list[n].first;
		if (var && (MV_DEFINED(&(var->v)) || n < lvzwrite_block->subsc_count -1))
		{
			lvzwr_var(var, n + 1);
			zwr_sub->subsc_list[n].actual = (mval *)NULL;
			lvzwrite_block->curr_subsc = n;
		} else
		{
			if (lvzwrite_block->fixed)
			{
				unsigned char buff[512], *end;

				lvzwrite_block->curr_subsc++;
				end = lvzwr_key(buff, SIZEOF(buff));
				zwr_sub->subsc_list[n].actual = (mval *)NULL;
				lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;
				rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
			}
		}
	} else  if (tbl = lv->ptrs.val_ent.children)
	{
		zwr_sub->subsc_list[n].actual = &mv;
		if (local_collseq_stdnull)
		{
			if (tbl->str && 0 == tbl->str->ptr.sbs_str[0].str.len)
			{	/* Process null subscript first */
				s = &tbl->str->ptr.sbs_str[0];
				COMMON_STR_PROCESSING;
			}
		}
		/** processing order of numeric subscripts are same for both GT.M collation and standard collation ***/
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
					COMMON_NUMERIC_PROCESSING;
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
					COMMON_NUMERIC_PROCESSING;
					if (do_lev)
						lvzwr_var(f->lv, n + 1);
				}
			}
		}

		mv.mvtype = MV_STR;
		/* For standard collation, if there is any null subscripts we have already processed that */
		if (blk = tbl->str) /* CAUTION: assignment */
		{
			s = &blk->ptr.sbs_str[0];
			if (0 == s->str.len && local_collseq_stdnull)
				s++;
			for(;;)
			{
				for (top = (char *)(&blk->ptr.sbs_str[0] + blk->cnt); s < (sbs_str_struct *)top; s++)
				{
					COMMON_STR_PROCESSING;
				}
				if (blk = blk->nxt) /* CAUTION: assignment */
					s = &blk->ptr.sbs_str[0];
				else
					break;
			}
		}
		zwr_sub->subsc_list[n].actual = (mval *)NULL;
		lvzwrite_block->curr_subsc = n;
	}
}
