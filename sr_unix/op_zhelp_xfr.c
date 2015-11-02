/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
static mval dlib 	= DEFINE_MVAL_STRING(MV_STR, 0 , 0 , SIZEOF("$gtm_dist/gtmhelp.gld") - 1 ,
					 (char *) "$gtm_dist/gtmhelp.gld" , 0 , 0 );

GBLREF spdesc stringpool;

void op_zhelp_xfr(mval *subject, mval *lib)
{
	mstr	x;
	mval	*action;

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

	mval_lex(subject, &x);
	if (x.addr == (char *)stringpool.free)
	{	action->str.len += x.len;
		stringpool.free += x.len;
	}
	else
		op_cat(VARLSTCNT(3) action, action, subject);

	op_cat(VARLSTCNT(3) action, action, &com);	/* add "," */

	mval_lex(lib, &x);
	if (x.addr == (char *)stringpool.free)
	{	action->str.len += x.len;
		stringpool.free += x.len;
	}
	else
		op_cat(VARLSTCNT(3) action, action, lib);

	op_cat(VARLSTCNT(3) action, action, &rpar);	/* add ")" */

	op_commarg(action,indir_linetail);
}

