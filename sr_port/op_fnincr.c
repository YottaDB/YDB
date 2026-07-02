/****************************************************************
 *								*
 * Copyright (c) 2004-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

#include "mvalconv.h"
#include "op.h"		/* for op_add prototype */

LITREF	mval	literal_null;

void	op_fnincr(lv_val *local_var, mval *increment, mval *result)
{
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;

	MV_FORCE_NUM(increment); /* we do this operation in op_gvincr(), so it should be no different in op_fnincr() */
	if (!MV_DEFINED(&local_var->v))
		local_var->v.umval = literal_null.umval;
	op_add(&local_var->v, increment, &local_var->v);
	result->umval = local_var->v.umval;
}
