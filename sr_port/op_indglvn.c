/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "cache.h"
#include "op.h"

GBLREF	bool	undef_inhibit;
GBLREF	symval	*curr_symval;
GBLREF	mval	**ind_result_sp, **ind_result_top;

LITREF	char	ctypetab[NUM_ASCII_CHARS];
LITREF	mval	literal_null;

void	op_indglvn(mval *v,mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	char		*i, *i_top, *c, *c_top;
	lv_val 		*a;
	ht_entry 	*q;
	int4		y;
	mident		ident;

	error_def(ERR_INDMAXNEST);
	error_def(ERR_UNDEF);

	MV_FORCE_STR(v);
	if (!(obj = cache_get(indir_glvn, &v->str)))
	{
		if (v->str.len && v->str.len <= sizeof(mident))
		{
			i = (char *)&ident;
			i_top = i + sizeof(ident);
			c = v->str.addr;
			c_top = c + v->str.len;
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
					{	if (undef_inhibit)
						{	*dst = literal_null;
							return;
						}
						else
							rts_error(VARLSTCNT(4) ERR_UNDEF, 2, v->str.len, v->str.addr);
					}
					assert (q->ptr);
					a = (lv_val *)q->ptr;
					*dst = a->v;
					return;
				}
			}
		}
		comp_init(&v->str);
		rval = glvn(&x);
		if (comp_fini(rval, &object, OC_IRETMVAL, &x, v->str.len))
		{
			cache_put(indir_glvn, &v->str, &object);
			*ind_result_sp++ = dst;
			if (ind_result_sp >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
			comp_indr(&object);
		}
	}
	else
	{
		*ind_result_sp++ = dst;
		if (ind_result_sp >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(obj);
	}
}
