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
#include "cli.h"
#include "dse.h"

void dse_remove(void)
{

	if (cli_present("RECORD") == CLI_PRESENT)
	{	dse_rmrec();
	}else if (cli_present("BLOCK") == CLI_PRESENT)
	{	dse_rmsb();
	}
	return;
}
