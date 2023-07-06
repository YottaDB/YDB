/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "io.h"
#include "iottdef.h"
#include "iott_setterm.h"
#include "error.h"
#include "util.h"

GBLREF io_log_name	*io_root_log_name;

CONDITION_HANDLER(io_init_ch)
{
	io_log_name	*iol;

	START_CH(TRUE);
#	ifdef VMS
	if (INFO == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
#	endif
	for (iol = io_root_log_name;  0 != iol;  iol = iol->next)
	{
		if (iol->iod && (iol->iod->type == tt) && iol->iod->dev_sp)
			iott_resetterm(iol->iod);
	}
	NEXTCH;
}
