/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "underr.h"
#include "mvalconv.h"

void op_fno2(lv_val *src,mval *key,mval *dst,mval *direct)
{
	error_def(ERR_ORDER2);

	MV_FORCE_DEFINED(key);
	MV_FORCE_NUM(direct);
	if (!MV_IS_INT(direct) || (direct->m[1] != 1*MV_BIAS && direct->m[1] != -1*MV_BIAS))
		rts_error(VARLSTCNT(1) ERR_ORDER2);
	else
	{	if (direct->m[1] == 1*MV_BIAS)
			op_fnorder(src,key,dst);
		else
			op_fnzprevious(src,key,dst);
	}
}
