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

#include "lv_val.h"
#include "compiler.h"
#include "opcode.h"
#include "op.h"
#include "gtm_string.h"
#include "callg.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "glvn_pool.h"

/* [Used by SET] */
void op_rfrshgvn(uint4 indx, opctype oc)
{
	glvn_pool_entry		*slot;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	slot = &(TREF(glvn_pool_ptr))->slot[indx];
	assert(MAX_ACTUALS >= slot->glvn_info.n);
	switch (oc)
	{
	case OC_GVNAME:
		callg((callgfnptr)op_gvname, (gparam_list *)&slot->glvn_info);
		break;
	case OC_GVNAKED:
		callg((callgfnptr)op_gvnaked, (gparam_list *)&slot->glvn_info);
		break;
	case OC_GVEXTNAM:
		callg((callgfnptr)op_gvextnam, (gparam_list *)&slot->glvn_info);
		break;
	default:
		GTMASSERT;
	}
}
