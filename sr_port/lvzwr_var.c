/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "lv_val.h"
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
#include "promodemo.h"	/* for "demote" prototype used in LV_NODE_GET_KEY */

#define eb_less(u, v)    (numcmp(u, v) < 0)

#define COMMON_STR_PROCESSING(NODE)										\
{														\
	mstr		key_mstr;										\
	mval 		tmp_sbs;										\
														\
	assert(MV_STR & mv.mvtype);										\
	if (TREF(local_collseq))										\
	{													\
		key_mstr = mv.str;										\
		mv.str.len = 0;	/* protect from "stp_gcol", if zwr_sub->subsc_list[n].actual points to mv */	\
		ALLOC_XFORM_BUFF(key_mstr.len);									\
		tmp_sbs.mvtype = MV_STR;									\
		tmp_sbs.str.len = TREF(max_lcl_coll_xform_bufsiz);						\
		assert(NULL != TREF(lcl_coll_xform_buff));							\
		tmp_sbs.str.addr = TREF(lcl_coll_xform_buff);							\
		do_xform(TREF(local_collseq), XBACK, &key_mstr, &tmp_sbs.str, &length);				\
		tmp_sbs.str.len = length;									\
		s2pool(&(tmp_sbs.str));										\
		mv.str = tmp_sbs.str;										\
	}													\
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
		lvzwr_var((lv_val *)NODE, n + 1);								\
}

#define COMMON_NUMERIC_PROCESSING(NODE)								\
{												\
	do_lev = TRUE;										\
	if (n < lvzwrite_block->subsc_count)							\
	{											\
		if (zwr_sub->subsc_list[n].subsc_type == ZWRITE_PATTERN)			\
		{										\
			if (!do_pattern(&mv, zwr_sub->subsc_list[n].first))			\
				do_lev = FALSE;							\
		} else  if (zwr_sub->subsc_list[n].subsc_type != ZWRITE_ALL)			\
		{										\
			if (zwr_sub->subsc_list[n].first)					\
			{									\
		 		if (!MV_IS_CANONICAL(zwr_sub->subsc_list[n].first)		\
						|| eb_less(&mv, zwr_sub->subsc_list[n].first))	\
					do_lev = FALSE;						\
			}									\
			if (do_lev && zwr_sub->subsc_list[n].second)				\
			{									\
				if (MV_IS_CANONICAL(zwr_sub->subsc_list[n].second)		\
						&& eb_less(zwr_sub->subsc_list[n].second, &mv))	\
					do_lev = FALSE;						\
			}									\
		}										\
	}											\
	if (do_lev)										\
		lvzwr_var((lv_val *)NODE, n + 1);						\
}

GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF int4		outofband;
GBLREF zshow_out	*zwr_output;
GBLREF int		merge_args;
GBLREF zwr_hash_table	*zwrhtab;			/* How we track aliases during zwrites */

LITREF	mval		literal_null;

error_def(ERR_UNDEF);

/* lv subscript usage notes:
 *  1. The sub field in lvzwrite_datablk is an array allocated at MAX_LVSUBSCRIPTS.
 *  2. The subscripts that appear at any given time are those for the current node being processed.
 *  3. Nodes are setup by lvzwr_arg().
 *
 * Example - take the following nodes:
 *   A(1,1)=10
 *   A(1,2)=20
 *
 * The simplified processing that occurs is as follows:
 *  1. lvzwr_fini() sets curr_name which is the base var name (A)
 *  2. First level lvzwr_var is called with level (aka n) == 0
 *  3. Since A has no value, nothing is printed. Notices that there are children so lvzwr_arg()
 *     is called recursively with level 1.
 *  4. Sets up the level 1 subscript (key = 1).
 *  5. Since A(1) has no value, nothing is printed. Notices that there are children so lvzwr_arg()
 *     is called recursively withe level 2.
 *  6. Sets up the level 2 subscript (key = 1).
 *  7. A(1,1) does have a value so lvzwr_out() is called to print the current key (from these
 *     subscripts) and its value.
 *  8. No more subscripts at this level so pops back to level 1.
 *  9. There is another child at this level so calls lvzwr_arg() recursively with level 2.
 * 10. Replaces the level 2 subscript with the new key value (key = 2).
 * 11. A(1,2) does have a value so lvzwr_out() is called to print the current key.
 * 12. no more children at any level so everything pops back.
 */
void lvzwr_var(lv_val *lv, int4 n)
{
	mval		mv;
	int             length;
	lv_val		*var;
	char		*top;
	int4		i;
	boolean_t	do_lev, verify_hash_add, htent_added, value_printed_pending;
	zwr_sub_lst	*zwr_sub;
	ht_ent_addr	*tabent_addr;
	zwr_alias_var	*zav, *newzav;
	lvTree		*lvt;
	lvTreeNode	*node, *nullsubsnode, *parent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	 * this is a base var (0 == lvzwrite_block->subsc_count) and the var is an alias.
	 *
	 * 1. Check if we have seen it before (the lvval is in the zwr_alias_var hash table), then we
	 *    need to process this var with lvzwr_out NOW and we will only be processing the base
	 *    var, not any of the subscripts. This is because all those subscripts (and the value
	 *    of the base var itself) have been dealt with previously when we first saw this
	 *    lvval. So in that case, call lvzwr_out() to output the association after which we are
	 *    done with this var.
	 * 2. If we haven't seen it before, set a flag so we verify if the base var gets processed by
	 *    lvzwr_out or not (i.e. whether it has a value and the "subscript" or lack there of is
	 *    either wildcarded or whatever so that it actually gets dumped by lvzwr_out (see conditions
	 *    below). If not, then *we* need to add the lvval to the hash table to signify we have seen
	 *    it before so the proper associations to this alias var can be printed at a later time
	 *    when/if they are encountered.
	 */
	verify_hash_add = FALSE;	/* By default we don't need to verify add */
	value_printed_pending = FALSE;	/* Force the "value_printed" flag on if TRUE */
	zav = NULL;
	if (!merge_args && LV_IS_BASE_VAR(lv) && IS_ALIASLV(lv))
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
	if (LV_IS_VAL_DEFINED(lv)
	    && (!lvzwrite_block->subsc_count || ((0 == n) && ZWRITE_ASTERISK == zwr_sub->subsc_list[n].subsc_type)
		|| ((0 != n) && !(lvzwrite_block->mask >> n))))
	{	/* Print value for *this* node  */
		lvzwr_out(lv);
	}
	if (verify_hash_add && !lvzwrite_block->zav_added)
	{	/* lvzwr_out processing didn't add a zav for this var. Take care of that now so we
		 * recognize it as a "dealt with" alias when/if it is encountered later.
		 */
		newzav = als_getzavslot();
		newzav->zwr_var = *lvzwrite_block->curr_name;
		newzav->value_printed = TRUE;
		htent_added = add_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lv, newzav, &tabent_addr);
		assert(htent_added);
	}
	/* If we processed a base var above to print an alias association but it hadn't been printed yet,
	 * we had to wait until after lvzwr_out() was called before we could set the flag that indicated
	 * the printing had occurred. Do that now. Note that it is only when this flag is set we are
	 * certain to have a good value in zav.
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
		if (var && (LV_IS_VAL_DEFINED(var) || n < lvzwrite_block->subsc_count -1))
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
	} else  if (lvt = LV_GET_CHILD(lv))
	{	/* If node has children, process them now */
		zwr_sub->subsc_list[n].actual = &mv;
		/* In case of standard null collation, first process null subscript if it exists */
		if (TREF(local_collseq_stdnull))
		{
			nullsubsnode = lvAvlTreeLookupStr(lvt, (treeKeySubscr *)&literal_null, &parent);
			if (NULL != nullsubsnode)
			{
				assert(MVTYPE_IS_STRING(nullsubsnode->key_mvtype) && !nullsubsnode->key_len);
				/* Process null subscript first */
				LV_STR_NODE_GET_KEY(nullsubsnode, &mv); /* Get node key into "mv" */
				COMMON_STR_PROCESSING(nullsubsnode);
			}
		} else
			nullsubsnode = NULL;
		for (node = lvAvlTreeFirst(lvt); NULL != node; node = lvAvlTreeNext(node))
		{
			if (node == nullsubsnode)
			{
				assert(TREF(local_collseq_stdnull));
				continue;	/* skip null subscript as it has already been processed */
			}
			LV_NODE_GET_KEY(node, &mv); /* Get node key into "mv" depending on the structure type of "node" */
			if (!MVTYPE_IS_STRING(mv.mvtype))
			{	/* "node" is of type "lvTreeNodeNum *" */
				COMMON_NUMERIC_PROCESSING(node);
			} else
			{	/* "node" is of type "lvTreeNode *" */
				COMMON_STR_PROCESSING(node);
			}
		}
		zwr_sub->subsc_list[n].actual = (mval *)NULL;
		lvzwrite_block->curr_subsc = n;
	}
}
