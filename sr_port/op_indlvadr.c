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
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "indir_enum.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"

GBLREF	hash_table_objcode	cache_table;
GBLREF	char			window_token;

error_def(ERR_VAREXPECTED);

void	op_indlvadr(mval *target)
{
	boolean_t	rval;
	icode_str	indir_src;
	mstr		object, *obj;
	oprtype		v;
	triple		*s;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_lvadr;
	TREF(for_ctrl_indr_subs) = FALSE;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&target->str);
		switch (window_token)
		{
		case TK_IDENT:
			rval = lvn(&v, OC_PUTINDX, NULL);
			s = v.oprval.tref;	/* this ugliness serves to return a flag compiled code can use to adjust flow */
			TREF(for_ctrl_indr_subs) = (OC_PUTINDX == s->opcode);
			if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
			{
				indir_src.str.addr = target->str.addr;
				cache_put(&indir_src, &object);
				comp_indr(&object);
				if (TREF(for_ctrl_indr_subs))	/* if subscripts (expect rare), don't cache so always set flag */
					delete_hashtab_objcode(&cache_table, &indir_src);
			}
			break;
		case TK_ATSIGN:
			if (rval = indirection(&v))
			{	/* if the indirection nests, for_ctrl_indr_subs doesn't matter until we get the "real" lvn */
				s = newtriple(OC_INDLVADR);
				s->operand[0] = v;
				v = put_tref(s);
				if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
				{
					indir_src.str.addr = target->str.addr;
					cache_put(&indir_src, &object);
					comp_indr(&object);
				}
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
	}
	else
		comp_indr(obj);
	return;
}
