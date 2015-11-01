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
#include "toktyp.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cache.h"
#include "do_indir_do.h"
#include "op.h"

#define INDIR(a, b, c) b()
int
#include "indir.h"
;
#undef INDIR
#define INDIR(a, b, c) b
LITDEF int (*indir_fcn[])() = {
#include "indir.h"
};

GBLREF char window_token;
GBLREF stack_frame *frame_pointer;
GBLREF unsigned char 	proc_act_type;

void	op_commarg(mval *v, unsigned char argcode)
{
	bool	rval;
	mstr	*obj, object;
	error_def	(ERR_INDEXTRACHARS);

	MV_FORCE_STR(v);
	assert(argcode >=3 && argcode < sizeof(indir_fcn) / sizeof(indir_fcn[0]));

 	/* Note cache_get call must come first in test below because we ALWAYS want it
 	   to be executed to set cache_hashent for the subsequent cache_put */
	if (!(obj = cache_get(argcode, &v->str)) || indir_linetail_nocache == argcode)
	{
		if (((indir_do == argcode) || (indir_goto == argcode)) &&
		    (frame_pointer->type & SFT_COUNT) && v->str.len && (v->str.len < sizeof(mident)) &&
		    !proc_act_type && do_indir_do(v, argcode))
		{
			return;
		}
		comp_init(&v->str);
		for (;;)
		{
			if (!(rval = (*indir_fcn[argcode])()))
				break;
			if (TK_EOL == window_token)
				break;
			if (TK_COMMA == window_token)
				advancewindow();
			else
				rts_error(VARLSTCNT(1) ERR_INDEXTRACHARS);
		}
		if (comp_fini(rval, &object, OC_RET, 0, v->str.len))
		{
			cache_put(argcode, &v->str, &object);
			comp_indr(&object);
			if (indir_linetail == argcode)
				frame_pointer->type = SFT_COUNT;
		}
	} else
	{
		comp_indr(obj);
		if (indir_linetail == argcode)
			frame_pointer->type = SFT_COUNT;
	}
}
