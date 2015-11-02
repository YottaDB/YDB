/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "mv_stent.h"
#include "mvalconv.h"
#include "dollar_zlevel.h"
#include "ztrap_save_ctxt.h"

GBLREF mv_stent *mv_chain;
GBLREF unsigned char *stackbase,*stacktop,*msp,*stackwarn;
GBLREF mval ztrap_pop2level;

void ztrap_save_ctxt(void)
{
	int level;
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	level = dollar_zlevel();
	if (level == MV_FORCE_INTD(&ztrap_pop2level))
		return;

	PUSH_MV_STENT(MVST_MSAV);
	mv_chain->mv_st_cont.mvs_msav.v = ztrap_pop2level;
	mv_chain->mv_st_cont.mvs_msav.addr = &ztrap_pop2level;
	MV_FORCE_MVAL(&ztrap_pop2level, level);
	return;
}
