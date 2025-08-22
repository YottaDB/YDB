/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "indir_enum.h"
#include "stringpool.h"
#include "op.h"
#include "io.h"

static mval com 	= DEFINE_MVAL_STRING(MV_STR, 0 , 0 , 1 , (char *) "," , 0 , 0 );
static mval rpar 	= DEFINE_MVAL_STRING(MV_STR, 0 , 0 , 1 , (char *) ")" , 0 , 0 );
static mval dlib 	= DEFINE_MVAL_STRING(MV_STR, 0 , 0 , SIZEOF("$ydb_dist/gtmhelp.gld") - 1 ,
					 (char *) "$ydb_dist/gtmhelp.gld" , 0 , 0 );

GBLREF spdesc stringpool;

void op_zhelp_xfr(mval *subject, mval *lib)
{
	mstr	x;
	mval	*action;
	int	i;

	MV_FORCE_STR(subject);
	MV_FORCE_STR(lib);
	if (!lib->str.len)
		lib = &dlib;

	flush_pio();
	action = push_mval(subject);
	action->mvtype = 0;
	action->str.len = SIZEOF("D ^GTMHELP(") - 1;
	action->str.addr = "D ^GTMHELP(";
	s2pool(&action->str);
	action->mvtype = MV_STR;

	/* To "action", we concatenate the following things in that order before calling "op_commarg()".
	 * - subject (after it has been processed by mval_lex())
	 * - com (a comma)
	 * - lib (after it has been processed by mval_lex())
	 * - rpar (right parentheses)
	 * To avoid code duplication, we do the above 4 steps in a for loop with 2 iterations where each iteration
	 * does 2 of the above steps.
	 */
	for (i = 0; i < 2; i++)
	{
		mval_lex((0 == i) ? subject : lib, &x);
		/* With "stp_gcol()", we are guaranteed that "action->str.addr" points to the end of the stringpool
		 * as that is the most recent string in the stringpool and any "stp_gcol()" call inside "mval_lex()"
		 * would continue to keep it at the end of the stringpool. But with "stp_gcol_nosort()", this is no
		 * longer guaranteed. String order in the stringpool is not preserved. And so we need to check if
		 * "action" and "x" line up one after the other in the stringpool and if so skip the heavyweight
		 * "op_cat()" call below. To keep the code simple, we do this check in both sort and nosort cases.
		 */
		if (IS_AT_END_OF_STRINGPOOL(action->str.addr, action->str.len) && IS_AT_END_OF_STRINGPOOL(x.addr, 0))
		{
			assert(!memcmp(action->str.addr + action->str.len, x.addr, x.len));
			action->str.len += x.len;
			stringpool.free += x.len;
		} else
		{
			mval	*mv;

			PUSH_MV_STENT(MVST_MVAL); /* create a temporary on M stack */
			mv = &mv_chain->mv_st_cont.mvs_mval;
			mv->mvtype = MV_STR;
			mv->str = x;
			stringpool.free += x.len;
			op_cat(VARLSTCNT(3) action, action, mv);
			POP_MV_STENT(); /* no more need of temporary */
		}
		op_cat(VARLSTCNT(3) action, action, (0 == i) ? &com : &rpar);
	}
	op_commarg(action,indir_linetail);
}
