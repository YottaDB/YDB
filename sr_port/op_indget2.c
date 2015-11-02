/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "op.h"
#include "lv_val.h"
#include "mvalconv.h"
#include "glvn_pool.h"

error_def(ERR_ORDER2);
error_def(ERR_VAREXPECTED);

void	op_indget2(mval *dst, uint4 indx)
{
	glvn_pool_entry	*slot;
	lv_val		*lv;
	opctype		oc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	slot = &((TREF(glvn_pool_ptr))->slot[indx]);
	oc = slot->sav_opcode;
	if (OC_SAVLVN == oc)
	{	/* lvn */
		lv = op_rfrshlvn(indx, OC_RFRSHLVN);	/* funky opcode prevents UNDEF in rfrlvn */
		op_fnget1((mval *)lv, dst);
	} else if (OC_NOOP != oc)			/* if indirect error blew set up, skip this */
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		op_fngvget1(dst);
	}
	return;
}
