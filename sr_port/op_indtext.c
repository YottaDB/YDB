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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "stringpool.h"
#include "toktyp.h"
#include "copy.h"
#include "advancewindow.h"
#include "cache.h"
#include "op.h"

GBLREF spdesc stringpool;
GBLREF char window_token;
GBLREF mident window_ident;
GBLREF mval **ind_result_sp, **ind_result_top;

void op_indtext(mval *v,mint offset,mval *dst)
{
	triple *ref, *label;
	mstr *obj, object, vprime;
	oprtype x;
	bool rval;
	error_def(ERR_TEXTARG);
	error_def(ERR_INDMAXNEST);

	MV_FORCE_STR(v);
	vprime = v->str;
	vprime.len += sizeof(mint);
	if (stringpool.top - stringpool.free < vprime.len)
		stp_gcol(vprime.len);
	memcpy(stringpool.free, vprime.addr, v->str.len);
	vprime.addr = (char *)stringpool.free;
	stringpool.free += v->str.len;
	PUT_LONG(stringpool.free,offset);
	stringpool.free += sizeof(mint);
	if (!(obj = cache_get(indir_text,&vprime)))
	{
		comp_init(&v->str);
		ref = maketriple(OC_FNTEXT);
		label = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(label);
		switch (window_token)
		{
		case TK_INTLIT:
			int_label();
			/* caution: fall through */
		case TK_IDENT:
			ref->operand[0] = put_str(&window_ident.c[0],sizeof(mident));
			advancewindow();
			rval = TRUE;
			break;
		case TK_ATSIGN:
			if (rval = indirection(&(ref->operand[0])))
				ref->opcode = OC_INDTEXT;
			break;
		default:
			stx_error(ERR_TEXTARG);
			rval = FALSE;
		}
		if (rval)
		{
			label->operand[0] = put_ilit(offset);
			label->operand[1] = put_tref(newtriple(OC_CURRTN));
			ins_triple(ref);
			x = put_tref(ref);
		}
		if (!comp_fini(rval, &object, OC_IRETMVAL, &x, v->str.len))
			return;
		cache_put(indir_text,&vprime,&object);
		*ind_result_sp++ = dst;
		if (ind_result_sp >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(&object);
		return;
	}
	*ind_result_sp++ = dst;
	if (ind_result_sp >= ind_result_top)
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
	comp_indr(obj);
	return;
}
