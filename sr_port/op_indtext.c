/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
#include "mv_stent.h"

GBLREF unsigned char 		*source_buffer;
GBLREF short int 		source_column;
GBLREF spdesc 			stringpool;
GBLREF mv_stent			*mv_chain;
GBLREF unsigned char		*msp, *stackwarn, *stacktop;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

void op_indtext(mval *lab, mint offset, mval *rtn, mval *dst)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	mval		mv_off;
	oprtype		opt, getdst;
	triple		*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(lab);
	indir_src.str.len = lab->str.len;
	indir_src.str.len += SIZEOF("+^") - 1;
	indir_src.str.len += MAX_NUM_SIZE;
	indir_src.str.len += rtn->str.len;
	ENSURE_STP_FREE_SPACE(indir_src.str.len);
	DBG_MARK_STRINGPOOL_UNEXPANDABLE; /* Now that we have ensured enough space in the stringpool, we dont expect any more
					   * garbage collections or expansions until we are done with the below initialization.
					   */
	/* Push an mval pointing to the complete entry ref on to the stack so the string is valid even
	 * if garbage collection occurs before cache_put
	 */
	PUSH_MV_STENT(MVST_MVAL);
	mv_chain->mv_st_cont.mvs_mval.mvtype = 0;	/* so stp_gcol, which may be invoked below, does not get confused by
							 * this otherwise incompletely initialized mval in the M-stack
							 */
	mv_chain->mv_st_cont.mvs_mval.str.addr = (char *)stringpool.free;
	memcpy(stringpool.free, lab->str.addr, lab->str.len);
	stringpool.free += lab->str.len;
	*stringpool.free++ = '+';
	MV_FORCE_MVAL(&mv_off, offset);
	MV_FORCE_STRD(&mv_off); /* goes at stringpool.free. we already made enough space in the stp_gcol call */
	*stringpool.free++ = '^';
	memcpy(stringpool.free, rtn->str.addr, rtn->str.len);
	stringpool.free += rtn->str.len;
	mv_chain->mv_st_cont.mvs_mval.str.len = INTCAST(stringpool.free - (unsigned char*)mv_chain->mv_st_cont.mvs_mval.str.addr);
	mv_chain->mv_st_cont.mvs_mval.mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	DBG_MARK_STRINGPOOL_EXPANDABLE;	/* Now that we are done with stringpool.free initializations, mark as free for expansion */
	indir_src.str = mv_chain->mv_st_cont.mvs_mval.str;
	indir_src.code = indir_text;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&indir_src.str, &getdst);
		rval = f_text(&opt, OC_FNTEXT);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &opt, &getdst, indir_src.str.len))
		{
			assert(MVST_MVAL == mv_chain->mv_st_type);
			POP_MV_STENT();
			return;
		}
		indir_src.str.addr = mv_chain->mv_st_cont.mvs_mval.str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;					/* Where to store return value */
	assert(MVST_MVAL == mv_chain->mv_st_type);
	POP_MV_STENT(); /* unwind the mval entry before the new frame gets added by comp_indir below */
	comp_indr(obj);
	return;
}
