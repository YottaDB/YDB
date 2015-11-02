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
#include "gtm_string.h"
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "indir_enum.h"
#include "cache.h"
#include "hashtab_mname.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "valid_mname.h"
#include "stack_frame.h"
#include "stringpool.h"

GBLREF	boolean_t		mstr_native_align;
GBLREF	hash_table_objcode	cache_table;
GBLREF	stack_frame		*frame_pointer;

error_def(ERR_VAREXPECTED);

void	op_indlvadr(mval *target)
{
	char		*ptr;
	icode_str	indir_src;
	int		rval;
	mname_entry	*targ_key;
	mstr		*obj, object;
	mval		*saved_indx;
	oprtype		v;
	triple		*s;
	uint4		align_padlen, len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_lvadr;
	saved_indx = NULL;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str);
		switch (TREF(window_token))
		{
		case TK_IDENT:
			rval = lvn(&v, OC_SAVPUTINDX, NULL);
			s = v.oprval.tref;	/* this ugliness serves to return a flag compiled code can use to adjust flow */
			if (OC_SAVPUTINDX != s->opcode)
			{	/* this block grabs a way to look up the name later and is similar to some code in op_savputindx */
				assert(MVAR_REF == s->operand->oprclass);
				saved_indx = (mval *)malloc(SIZEOF(mval) + SIZEOF(mname_entry) + SIZEOF(mident_fixed));
				saved_indx->mvtype = MV_STR;
				ptr = (char *)saved_indx + SIZEOF(mval);
				saved_indx->str.addr = ptr;
				targ_key = (mname_entry *)ptr;
				ptr += SIZEOF(mname_entry);
				targ_key->var_name.addr = ptr;
				len = s->operand[0].oprval.vref->mvname.len;
				assert(SIZEOF(mident_fixed) > len);
				memcpy(ptr, s->operand[0].oprval.vref->mvname.addr, len);
				targ_key->var_name.len = len;
				saved_indx->str.len = SIZEOF(mname_entry) + len;
				COMPUTE_HASH_MNAME(targ_key);
				targ_key->marked = FALSE;
				MANAGE_FOR_INDX(frame_pointer, TREF(for_nest_level), saved_indx);
			}
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))	/* NOTE assignment */
			{	/* if the indirection nests, for_ctrl_indr_subs doesn't matter until we get the "real" lvn */
				s = newtriple(OC_INDLVADR);
				s->operand[0] = v;
				v = put_tref(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAD, &v, target->str.len))
			return;
		/* before cache and execute, tack a little something on at end of object */
		assert(indir_src.str.addr == target->str.addr);
		len = SIZEOF(uint4) * 2;
		if (NULL != saved_indx)
			len += SIZEOF(mval) + SIZEOF(mname_entry) + SIZEOF(mident_fixed); /* overlength, but ends aligned */
		align_padlen = mstr_native_align ? PADLEN(stringpool.free, NATIVE_WSIZE) : 0;
		len += align_padlen;
		ptr = obj->addr + obj->len + align_padlen;
		assert((char *)stringpool.free - align_padlen == ptr);
		assert(ptr + len <= (char *)stringpool.top); /* ind_code, called by comp_fini, reserves to prevent gc */
		if (NULL != saved_indx)
		{	/* it's an unsubscripted name, so save the name infomation with the cached object */
			memcpy(ptr, (char *)saved_indx, SIZEOF(mval) + saved_indx->str.len);
			ptr += (len - (SIZEOF(uint4) * 2));
			*(uint4 *)ptr = align_padlen;
		}
		ptr += SIZEOF(uint4);
		*(uint4 *)ptr = len;
		stringpool.free += len;
		assert((ptr + SIZEOF(uint4)) == (char *)stringpool.free);
		obj->len += len;
		cache_put(&indir_src, obj);		/* this copies the "extended" object to the cache */
	} else
	{	/* if cached, the object has stuff at the end that might need pulling into the run-time context */
		ptr = (char *)(obj->addr + obj->len);
		len = *(uint4 *)(ptr - SIZEOF(uint4));
		if (SIZEOF(mval) < len)				/* not nested and not subscripted ? */
		{	/* grab the name information at the end of the cached indirect object and copy it to be useful to FOR */
			align_padlen = *(uint4 *)(ptr - (SIZEOF(uint4) * 2));
			assert(NATIVE_WSIZE > align_padlen);
			assert(SIZEOF(mval) + SIZEOF(mname_entry) + SIZEOF(mident_fixed) + (SIZEOF(uint4) * 2)
				+ NATIVE_WSIZE > len);
			ptr -= (len + align_padlen);
			saved_indx = (mval *)ptr;
			assert(MV_STR == saved_indx->mvtype);
			len = SIZEOF(mval) + saved_indx->str.len;
			ptr = malloc(len);
			memcpy(ptr, (char *)saved_indx, len);
			saved_indx = (mval *)ptr;
			ptr += SIZEOF(mval);
			saved_indx->str.addr = ptr;
			assert(MAX_MIDENT_LEN >= ((mname_entry *)(saved_indx->str.addr))->var_name.len);
			assert((SIZEOF(mname_entry) + ((mname_entry *)(saved_indx->str.addr))->var_name.len)
				== saved_indx->str.len);
			ptr += SIZEOF(mname_entry);
			((mname_entry *)(saved_indx->str.addr))->var_name.addr = ptr;
			len = SIZEOF(mval) + SIZEOF(mname_entry) + SIZEOF(mident_fixed) + (SIZEOF(uint4) * 2);
			assert(*(uint4 *)(obj->addr + obj->len - SIZEOF(uint4)) == len);
			MANAGE_FOR_INDX(frame_pointer, TREF(for_nest_level), saved_indx);
		}
	}
	comp_indr(obj);
	return;
}
