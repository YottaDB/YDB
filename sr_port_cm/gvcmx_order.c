/****************************************************************
 *								*
 * Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "mvalconv.h"

bool gvcmx_order(void)
{
	mval v;

	ASSERT_IS_LIBGNPCLIENT;
	gvcmz_doop(CMMS_Q_ORDER, CMMS_R_ORDER, &v);
	if (MV_FORCE_INTD(&v))
		return TRUE;
	else
		return FALSE;
}
