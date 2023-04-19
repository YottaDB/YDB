/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_downgrade.c: Driver program to downgrade database files - Unsupported since V7 */

#include "mdef.h"
#include "mupip_exit.h"
#include "mupip_downgrade.h"

error_def(ERR_GTMCURUNSUPP);

void mupip_downgrade(void)
{
	mupip_exit(ERR_GTMCURUNSUPP);
}
