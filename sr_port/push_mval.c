/****************************************************************
 *								*
 * Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mv_stent.h"

GBLREF mv_stent 	*mv_chain;
GBLREF unsigned char 	*stackbase,*stacktop,*msp,*stackwarn;

mval *push_mval(arg1)
mval *arg1;
{
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	PUSH_MV_STENT(MVST_MVAL);
	mv_chain->mv_st_cont.mvs_mval = *arg1;
	return &mv_chain->mv_st_cont.mvs_mval;
}
