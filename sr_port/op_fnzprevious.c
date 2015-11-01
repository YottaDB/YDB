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
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "collseq.h"
#include "stringpool.h"
#include "op.h"
#include "do_xform.h"
#include "numcmp.h"
#include "mvalconv.h"
#include "underr.h"

LITREF	mval SBS_MVAL_INT_ELE;

GBLREF collseq		*local_collseq;
GBLREF char		*lcl_coll_xform_buff;

void op_fnzprevious (lv_val *src, mval *key, mval *dst)
{
	int		cur_subscr;
	mval		tmp_sbs;
	int             length;
	sbs_blk			*num, *str;
	bool			found;
	int4			i;
	lv_val			**lvpp;
	lv_sbs_tbl		*tbl;
	mstr			*strp;
	mflt			*fltp;

	found = FALSE;
	if (src)
	{
		if (tbl = src->ptrs.val_ent.children)
		{	MV_FORCE_DEFINED(key);
			num = tbl->num;
			str = tbl->str;
			assert (tbl->ident == MV_SBS);
			if (MV_IS_STRING(key) && key->str.len == 0)
			{
				if (str)
				{
					assert (str->cnt);
					while (str->nxt) str = str->nxt;
					dst->mvtype = MV_STR;
					dst->str = str->ptr.sbs_str[str->cnt - 1].str;
					found = TRUE;
				}
			}
			else
			{
				if (!MV_IS_CANONICAL(key))
				{
					if (local_collseq)
					{
						tmp_sbs.mvtype = MV_STR;
						tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
						assert(NULL != lcl_coll_xform_buff);
						tmp_sbs.str.addr = lcl_coll_xform_buff;
						do_xform(local_collseq->xform, &key->str, &tmp_sbs.str, &length);
						tmp_sbs.str.len = length;
						s2pool(&(tmp_sbs.str));
						key = &tmp_sbs;
					}
					if (str && (strp = lv_prv_str_inx (str, &key->str)))
					{
						dst->str = *strp;
						dst->mvtype = MV_STR;
						found = TRUE;
					}
				}
				else
				{	MV_FORCE_NUM(key);
					if (!tbl->int_flag)
					{
						if (num && (fltp = lv_prv_num_inx (num, key)))
						{
							MV_ASGN_FLT2MVAL((*dst),(*fltp)) ;
							found = TRUE;
						}
					}
					else
					{
						assert (num);
						if ( key->m[1] > 0 )
						{
							if (numcmp(key,&SBS_MVAL_INT_ELE)==1)
								i = SBS_NUM_INT_ELE;
							else
								{
								i = MV_FORCE_INT(key) ;
								if (!MV_IS_INT(key)) i++;
								}
							for (lvpp = &num->ptr.lv[i]; 0 <= --i && !*--lvpp; );
							if (0 <= i)
							{
								MV_FORCE_MVAL(dst,i) ;
								found = TRUE;
							}
						}
					}
					if (!found)
					{
						dst->mvtype = MV_STR;
						dst->str.len = 0;
						found = TRUE;
					}
				}
			}
			if (!found && num)
			{
				assert (num->cnt);
				if (!tbl->int_flag)
				{
					while (num->nxt) num = num->nxt;
					MV_ASGN_FLT2MVAL((*dst),num->ptr.sbs_flt[num->cnt - 1].flt) ;
					found = TRUE;
				}
				else
				{
					for (i = SBS_NUM_INT_ELE, lvpp = &num->ptr.lv[SBS_NUM_INT_ELE];
						(--i, num->ptr.lv <= --lvpp) && !*lvpp; );
					if (num->ptr.lv <= lvpp)
					{
						MV_FORCE_MVAL(dst,i) ;
						found = TRUE;
					}
				}
			}
		}
	}
	if (!found)
	{
		dst->mvtype = MV_STR;
		dst->str.len = 0;
	} else if (dst->mvtype == MV_STR && local_collseq)
	{
		assert(NULL != lcl_coll_xform_buff);
		tmp_sbs.str.addr = lcl_coll_xform_buff;
		tmp_sbs.str.len = MAX_LCL_COLL_XFORM_BUFSIZ;
		do_xform(local_collseq->xback, &dst->str, &tmp_sbs.str, &length);
		tmp_sbs.str.len = length;
		s2pool(&(tmp_sbs.str));
		dst->str = tmp_sbs.str;
	}
	return;
}
