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

#include "gtm_string.h"

#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "compiler.h"
#include "copy.h"
#include "indir_enum.h"
#include "mvalconv.h"
#include "op.h"
#include "opcode.h"
#include "stringpool.h"
#include "toktyp.h"
#include "rtnhdr.h"
#include "mv_stent.h"

GBLREF mval 			**ind_result_sp, **ind_result_top;
GBLREF unsigned char 		*source_buffer;
GBLREF short int 		source_column;
GBLREF spdesc 			stringpool;
GBLREF mv_stent			*mv_chain;
GBLREF unsigned char		*msp, *stackwarn, *stacktop;

void op_indtext(mval *lab, mint offset, mval *rtn, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	mval		mv_off;
	oprtype		opt;
	triple		*ref;
	icode_str	indir_src;

	error_def(ERR_INDMAXNEST);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	MV_FORCE_STR(lab);
	indir_src.str.len = lab->str.len;
	indir_src.str.len += SIZEOF("+^") - 1;
	indir_src.str.len += MAX_NUM_SIZE;
	indir_src.str.len += rtn->str.len;
	if (stringpool.top - stringpool.free < indir_src.str.len)
		stp_gcol(indir_src.str.len);

	/* Push an mval pointing to the complete entry ref on to the stack so the string is valid even
	 * if garbase collection occurs before cache_put() */
	PUSH_MV_STENT(MVST_MVAL);
	mv_chain->mv_st_cont.mvs_mval.mvtype = MV_STR;
	mv_chain->mv_st_cont.mvs_mval.str.addr = (char *)stringpool.free;
	memcpy(stringpool.free, lab->str.addr, lab->str.len);
	stringpool.free += lab->str.len;
	*stringpool.free++ = '+';
	MV_FORCE_MVAL(&mv_off, offset);
	MV_FORCE_STRD(&mv_off); /* goes at stringpool.free. we already made enough space in the stp_gcol() call */
	*stringpool.free++ = '^';
	memcpy(stringpool.free, rtn->str.addr, rtn->str.len);
	stringpool.free += rtn->str.len;
	mv_chain->mv_st_cont.mvs_mval.str.len = INTCAST(stringpool.free - (unsigned char*)mv_chain->mv_st_cont.mvs_mval.str.addr);

	indir_src.str = mv_chain->mv_st_cont.mvs_mval.str;
	indir_src.code = indir_text;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&indir_src.str);
		rval = f_text(&opt, OC_FNTEXT);
		if (!comp_fini(rval, &object, OC_IRETMVAL, &opt, indir_src.str.len))
		{
			assert(mv_chain->mv_st_type == MVST_MVAL);
			POP_MV_STENT();
			return;
		}
		indir_src.str.addr = mv_chain->mv_st_cont.mvs_mval.str.addr;
		cache_put(&indir_src, &object);
		*ind_result_sp++ = dst;
		if (ind_result_sp >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		assert(mv_chain->mv_st_type == MVST_MVAL);
		POP_MV_STENT(); /* unwind the mval entry before the new frame gets added by comp_indir below */
		comp_indr(&object);
		return;
	}
	*ind_result_sp++ = dst;
	if (ind_result_sp >= ind_result_top)
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
	assert(mv_chain->mv_st_type == MVST_MVAL);
	POP_MV_STENT(); /* unwind the mval entry before the new frame gets added by comp_indir below */
	comp_indr(obj);
	return;
}
