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

#include "advancewindow.h"
#include "cache.h"
#include "compiler.h"
#include "copy.h"
#include "indir_enum.h"
#include "mvalconv.h"
#include "op.h"
#include "opcode.h"
#include "stringpool.h"
#include "toktyp.h"

GBLREF mval **ind_result_sp, **ind_result_top;
GBLREF unsigned char *source_buffer;
GBLREF short int source_column;
GBLREF spdesc stringpool;

void op_indtext(mval *lab, mint offset, mval *rtn, mval *dst)
{
	bool	rval;
	mstr	*obj, object, vprime;
	mval	mv_off;
	oprtype	opt;
	triple	*ref;

	error_def(ERR_INDMAXNEST);

	MV_FORCE_STR(lab);
	vprime = lab->str;
	vprime.len += sizeof("+^") - 1;
	vprime.len += MAX_NUM_SIZE;
	vprime.len += rtn->str.len;
	if (stringpool.top - stringpool.free < vprime.len)
		stp_gcol(vprime.len);
	memcpy(stringpool.free, vprime.addr, lab->str.len);
	vprime.addr = (char *)stringpool.free;
	stringpool.free += lab->str.len;
	*stringpool.free++ = '+';
	MV_FORCE_MVAL(&mv_off, offset);
	MV_FORCE_STR(&mv_off);		/* goes at stringpool.free */
	*stringpool.free++ = '^';
	memcpy(stringpool.free, rtn->str.addr, rtn->str.len);
	stringpool.free += rtn->str.len;
	vprime.len = stringpool.free - (unsigned char*)vprime.addr;
	if (!(obj = cache_get(indir_text, &vprime)))
	{
		comp_init(&vprime);
		rval = f_text(&opt, OC_FNTEXT);
		if (!comp_fini(rval, &object, OC_IRETMVAL, &opt, vprime.len))
			return;
		cache_put(indir_text, &vprime, &object);
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
