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
#include "mdq.h"
#include "cache.h"
#include "op.h"
#include "underr.h"

GBLREF symval *curr_symval;
GBLREF char window_token;
GBLREF mval **ind_source_sp, **ind_source_top;
GBLREF mval **ind_result_sp, **ind_result_top;
GBLREF bool shift_gvrefs;
GBLREF triple *expr_start;

LITREF char ctypetab[128];

void	op_indget(mval *dst, mval *target, mval *value)
{
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	char		*i, *i_top, *c, *c_top;
	lv_val 		*a;
	int4		y;
	mident		ident;
	ht_entry 	*q;
	triple		*s, *src, *oldchain, tmpchain, *r, *triptr;

	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	if (!(obj = cache_get(indir_get, &target->str)))
	{
		if (target->str.len && target->str.len <= sizeof(mident))
		{
			i = (char *)&ident;
			i_top = i + sizeof(ident);
			c = target->str.addr;
			c_top = c + target->str.len;
			if (*c >= 0 && *c <= 127 && ((y = ctypetab[*i++ = *c++]) == TK_UPPER || y == TK_LOWER || y == TK_PERCENT))
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
					q = ht_get(&curr_symval->h_symtab , (mname *)&ident);
					if (!q || !MV_DEFINED(&((lv_val *)q->ptr)->v))
					{	*dst = *value;
					}
					else
					{
						assert (q->ptr);
						a = (lv_val *)q->ptr;
						*dst = a->v;
					}
					return;
				}
			}
		}
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		switch (window_token)
		{
		case TK_IDENT:
			if (rval = lvn(&v, OC_SRCHINDX, 0))
			{
				s = newtriple(OC_FNGET2);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
			}
			break;
		case TK_CIRCUMFLEX:
			if (rval = gvn())
			{
				s = newtriple(OC_FNGVGET);
				s->operand[0] = put_tref(src);
			}
			break;
		case TK_ATSIGN:
			if (shift_gvrefs)
			{
				dqinit(&tmpchain, exorder);
				oldchain = setcurtchain(&tmpchain);
				if (rval = indirection(&v))
				{
					s = newtriple(OC_INDGET);
					s->operand[0] = v;
					s->operand[1] = put_tref(src);
					newtriple(OC_GVSAVTARG);
					setcurtchain(oldchain);
					dqadd(expr_start, &tmpchain, exorder);
					expr_start = tmpchain.exorder.bl;
					triptr = newtriple(OC_GVRECTARG);
					triptr->operand[0] = put_tref(expr_start);
				}
				else
				{	setcurtchain(oldchain);
				}
			}
			else
			{
				if (rval = indirection(&v))
				{
					s = newtriple(OC_INDGET);
					s->operand[0] = v;
					s->operand[1] = put_tref(src);
				}
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
		v = put_tref(s);
		if (comp_fini(rval, &object, OC_IRETMVAL, &v, target->str.len))
		{	cache_put(indir_get, &target->str, &object);
			if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

			*ind_result_sp++ = dst;
			*ind_source_sp++ = value;
			comp_indr(&object);
		}
	}
	else
	{
		if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

		*ind_result_sp++ = dst;
		*ind_source_sp++ = value;
		comp_indr(obj);
	}
}
