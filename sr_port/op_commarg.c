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

#include "compiler.h"
#include "cmd.h"
#include "toktyp.h"
#include "rtnhdr.h"
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

GBLREF char			window_token;
GBLREF stack_frame		*frame_pointer;
GBLREF unsigned short 		proc_act_type;

void	op_commarg(mval *v, unsigned char argcode)
{
	bool		rval;
	mstr		*obj, object;
	icode_str	indir_src;
	error_def	(ERR_INDEXTRACHARS);

	MV_FORCE_STR(v);
	assert(argcode >=3 && argcode < SIZEOF(indir_fcn) / SIZEOF(indir_fcn[0]));
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		if (((indir_do == argcode) || (indir_goto == argcode)) &&
		    (frame_pointer->type & SFT_COUNT) && v->str.len && (v->str.len < MAX_MIDENT_LEN) &&
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
			{	/* Allow trailing spaces/comments that we will ignore */
				while (TK_SPACE == window_token)
					advancewindow();
				if (TK_EOL == window_token)
					break;
				rts_error(VARLSTCNT(1) ERR_INDEXTRACHARS);
			}
		}
		if (comp_fini(rval, &object, OC_RET, 0, v->str.len))
		{
			indir_src.str.addr = v->str.addr;	/* we reassign because v->str.addr
								might have been changed by stp_gcol() */
			cache_put(&indir_src, &object);
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
