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

#include "compiler.h"
#include "cmd.h"
#include "toktyp.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "do_indir_do.h"
#include "op.h"


#define INDIR(a, b, c) b
UNIX_ONLY(GBLDEF) VMS_ONLY(LITDEF) int (*indir_fcn[])() = {
#include "indir.h"
};

GBLREF stack_frame		*frame_pointer;
GBLREF unsigned short 		proc_act_type;

error_def	(ERR_INDEXTRACHARS);

void	op_commarg(mval *v, unsigned char argcode)
{
	int		rval;
	icode_str	indir_src;
	mstr		*obj, object;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	assert((3 <= argcode) && (SIZEOF(indir_fcn) / SIZEOF(indir_fcn[0]) > argcode));
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (NULL == (obj = cache_get(&indir_src)))	/* NOTE assignment */
	{
		obj = &object;
		if (((indir_do == argcode) || (indir_goto == argcode)) &&
		    (frame_pointer->type & SFT_COUNT) && v->str.len && (MAX_MIDENT_LEN > v->str.len) &&
		    !proc_act_type && do_indir_do(v, argcode))
			return;
		comp_init(&v->str, NULL);
		for (;;)
		{
			if (EXPR_FAIL == (rval = (*indir_fcn[argcode])()))	/* NOTE assignment */
				break;
			if (TK_EOL == TREF(window_token))
				break;
			if (TK_COMMA == TREF(window_token))
				advancewindow();
			else
			{	/* Allow trailing spaces/comments that we will ignore */
				while (TK_SPACE == TREF(window_token))
					advancewindow();
				if (TK_EOL == TREF(window_token))
					break;
				rts_error(VARLSTCNT(1) ERR_INDEXTRACHARS);
			}
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_RET, NULL, NULL, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;	/* reassign because stp_gcol might have changed v->str.addr */
		cache_put(&indir_src, obj);
	}
	comp_indr(obj);
	if (indir_linetail == argcode)
		frame_pointer->type = SFT_COUNT;
}
