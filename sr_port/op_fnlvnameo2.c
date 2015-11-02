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
#include "op.h"
#include "mvalconv.h"

void op_fnlvnameo2(mval *src,mval *dst,mval *direct)
{
	error_def(ERR_ORDER2);

	MV_FORCE_STR(src);
	MV_FORCE_NUM(direct);
	if (!MV_IS_INT(direct) || (direct->m[1] != (1 * MV_BIAS) && direct->m[1] != (-1 * MV_BIAS)))
		rts_error(VARLSTCNT(1) ERR_ORDER2);
	else
	{	if (direct->m[1] == (1 * MV_BIAS))
			op_fnlvname(src, FALSE, dst);
		else
			op_fnlvprvname(src, dst);
	}
}
