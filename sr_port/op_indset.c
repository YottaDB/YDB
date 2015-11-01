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
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include "underr.h"

GBLREF symval *curr_symval;
LITREF char ctypetab[128];
GBLREF char	window_token;
GBLREF mval **ind_source_sp, **ind_source_top;

void	op_indset(mval *target, mval *value)
{
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s, *src;
	char		*i, *i_top, *c, *c_top;
	ht_entry 	*q;
	lv_val		*a;
	char 		new;
	int4		y;
	mident		ident;

	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	if (!(obj = cache_get(indir_set, &target->str)))
	{
		if (target->str.len && target->str.len <= sizeof(mident))
		{
			i = (char *)&ident;
			i_top = i + sizeof(ident);
			c = target->str.addr;
			c_top = c + target->str.len;
			if ((y = ctypetab[*i++ = *c++]) == TK_UPPER || y == TK_LOWER || y == TK_PERCENT)
			{
				for ( ; c < c_top; c++,i++)
				{	y = ctypetab[*i = *c];
					if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
					{	break;
					}
				}
				if (c == c_top)
				{	/* we have an ident */
					for (; i < i_top; i++)
					{	*i = 0;
					}
					q = ht_put(&curr_symval->h_symtab , (mname *)&ident , &new);
					if (new)
					{	lv_newname(q, curr_symval);
					}
					a = (lv_val *)q->ptr;
					a->v = *value;
					return;
				}
			}
		}
		comp_init(&target->str);
		src = maketriple(OC_IGETSRC);
		ins_triple(src);
		switch (window_token)
		{
		case TK_IDENT:
			if (rval = lvn(&v, OC_PUTINDX, 0))
			{
				s = maketriple(OC_STO);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_CIRCUMFLEX:
			if (rval = gvn())
			{
				s = maketriple(OC_GVPUT);
				s->operand[0] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_ATSIGN:
			if (rval = indirection(&v))
			{
				s = maketriple(OC_INDSET);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
		if (comp_fini(rval, &object, OC_RET, 0, target->str.len))
		{	cache_put(indir_set, &target->str, &object);
			*ind_source_sp++ = value;
			if (ind_source_sp >= ind_source_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
			comp_indr(&object);
		}
	}
	else
	{ 	*ind_source_sp++ = value;
		if (ind_source_sp >= ind_source_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(obj);
	}
}
