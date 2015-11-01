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
#include "mv_stent.h"
#include "op.h"

GBLREF mv_stent *mv_chain;
GBLREF unsigned char *stackbase,*stacktop,*msp,*stackwarn;
GBLREF mval dollar_ztrap;

void op_newzt(void)
{
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	PUSH_MV_STENT(MVST_MSAV);
	mv_chain->mv_st_cont.mvs_msav.v = dollar_ztrap;
	mv_chain->mv_st_cont.mvs_msav.addr = &dollar_ztrap;
	dollar_ztrap.mvtype = MV_STR;
	dollar_ztrap.str.len = 0;
	return;
}
