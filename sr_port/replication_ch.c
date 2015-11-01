/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "error.h"

GBLREF	bool gv_replication_error;

CONDITION_HANDLER(replication_ch)
{
	START_CH;
	gv_replication_error = TRUE;
	if (SEVERITY == SEVERE)
	{
		NEXTCH;
	} else
		UNWIND(NULL, NULL);
}
