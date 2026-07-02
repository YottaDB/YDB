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
#include "op.h"
#include "gcol_list.h"

LITREF mval		literal_null;

/* This gets a src from op_fnget1 or op_fngvget1 which either contains the "gotten" value or is undefined, in which case this
 * returns the specified default value; this slight of hand deals with order of evaluation issues.
 */
void op_fnget2(mval *src, mval *def, mval *dst)
{
	MV_FORCE_DEFINED(def);
	dst->umval = MV_DEFINED(src) ? src->umval : def->umval;
	assert(0 == (dst->mvtype & MV_ALIASCONT));	/* Should be no alias container flag */
	return;
}
