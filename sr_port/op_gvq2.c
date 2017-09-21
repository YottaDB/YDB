/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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

error_def(ERR_QUERY2);

/* This function is basically a 2-argument $query(gvn,dir) call where the first argument is a gvn
 * and the 2nd argument dir is not a literal constant (so direction is not known at compile time in "f_query").
 * In this case, "f_query" generates an OC_GVQ2 opcode that invokes "op_gvq2" with the direction parameter evaluated
 * and so we can now decide whether to go with forward or reverse query of gvn.
 */
void op_gvq2(mval *dst,mval *direct)
{
	int4	dummy_intval;

	MV_FORCE_NUM(direct);
	if (!MV_IS_TRUEINT(direct, &dummy_intval) || (direct->m[1] != (1 * MV_BIAS) && direct->m[1] != (-1 * MV_BIAS)))
		rts_error(VARLSTCNT(1) ERR_QUERY2);
	else
		if (direct->m[1] == (1 * MV_BIAS))
			op_gvquery(dst);
		else
			op_gvreversequery(dst);
	}
}
