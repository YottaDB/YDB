/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "collseq.h"
#include "stringpool.h"
#include "do_xform.h"
#include "undx.h"
#include "mvalconv.h"
#include "val_iscan.h"

#define IS_INTEGER 0

GBLREF collseq		*local_collseq;
GBLREF bool		undef_inhibit;
LITREF mval		literal_null ;

lv_val	*op_getindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	mval			tmp_sbs;
	int			cur_subscr;
	int                     length;
	va_list			var;
	VMS_ONLY(int		argcnt;)
		int4			temp;
	lv_sbs_tbl     		*tbl;
	lv_val			*lv;
  	sbs_search_status      	status;
	mval			*key;
	int			arg1;
	unsigned char		buff[512], *end;

	error_def(ERR_UNDEF);

	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt));

	if (local_collseq)
		tmp_sbs.mvtype = MV_STR;

	lv = start;
	arg1 = --argcnt;
	cur_subscr = 1;
	while (lv  &&  argcnt-- > 0)
	{
		cur_subscr++;
		key = va_arg(var, mval *);
		if (NULL == (tbl = lv->ptrs.val_ent.children))
			lv = NULL;
		else
		{
			assert(tbl->ident == MV_SBS);
			if (!(key->mvtype & MV_NM ? !(key->mvtype & MV_NUM_APPROX) : (bool)val_iscan(key)))
			{
				if (local_collseq)
				{
					ALLOC_XFORM_BUFF(&key->str);
					tmp_sbs.mvtype = MV_STR;
					tmp_sbs.str.len = max_lcl_coll_xform_bufsiz;
					assert(NULL != lcl_coll_xform_buff);
					tmp_sbs.str.addr = lcl_coll_xform_buff;
					do_xform(local_collseq, XFORM, &key->str, &tmp_sbs.str, &length);
					tmp_sbs.str.len = length;
					s2pool(&(tmp_sbs.str));
					key = &tmp_sbs;
				}
				lv = (tbl->str) ? lv_get_str_inx(tbl->str, &key->str, &status) : NULL;
			} else
			{
				MV_FORCE_NUM(key);
				if (tbl->int_flag)
				{
					assert(tbl->num);
					if (MV_IS_INT(key))
					{
						temp = MV_FORCE_INT(key) ;
						lv = (temp >= 0 && temp < SBS_NUM_INT_ELE ? tbl->num->ptr.lv[temp] : NULL) ;
					} else
						lv = NULL;
			 	} else
					lv = (tbl->num) ? lv_get_num_inx(tbl->num, key, &status) : NULL;
			}
		}
	}
	va_end(var);
	if (!lv  ||  !MV_DEFINED(&lv->v))
	{
		if (undef_inhibit)
			lv = (lv_val *)&literal_null;
		else
		{
			VAR_START(var, start);
			end = undx(start, var, arg1, buff, SIZEOF(buff));
			va_end(var);
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
		}
	}
	return lv;
}
