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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"

bool gvcmx_get(mval *v)
{
	mval temp = {{0}};

	temp.mvtype = 0;
	gvcmz_doop(CMMS_Q_GET, CMMS_R_GET, &temp);
	if (MV_DEFINED(&temp))
		v->umval = temp.umval;

	return MV_DEFINED(&temp);
}
