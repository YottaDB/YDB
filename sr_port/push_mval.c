/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "gcol_list.h"

GBLREF mv_stent 	*mv_chain;
GBLREF unsigned char 	*stackbase,*stacktop,*msp,*stackwarn;
GBLREF spdesc		rts_stringpool, indr_stringpool;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

mval *push_mval(mval *arg1)
{
	PUSH_MV_STENT(MVST_MVAL);
	assert(glist_str_protected(&mv_chain->mv_st_cont.mvs_mval.str)); /* Auto-protected like locals */
	mv_chain->mv_st_cont.mvs_mval.umval = arg1->umval;
	return &mv_chain->mv_st_cont.mvs_mval;
}
