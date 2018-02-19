/****************************************************************
 *								*
 * Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

void gvcmx_kill(bool do_subtree)
{
	ASSERT_IS_LIBGNPCLIENT;
	if (do_subtree)
		gvcmz_doop(CMMS_Q_KILL, CMMS_R_KILL, 0);
	else
		gvcmz_doop(CMMS_Q_ZWITHDRAW, CMMS_R_ZWITHDRAW, 0);
}
