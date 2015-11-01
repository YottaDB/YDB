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

#include "gtm_string.h"

#include "io.h"
#include "iottdef.h"
#include "error.h"
#include "setterm.h"
#include "util.h"

GBLREF io_log_name	*io_root_log_name;

CONDITION_HANDLER(io_init_ch)
{
	io_log_name	*iol;

	START_CH;
	if (INFO == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
	for (iol = io_root_log_name;  0 != iol;  iol = iol->next)
	{
		if (iol->iod && (iol->iod->type == tt) && iol->iod->dev_sp)
			resetterm(iol->iod);
	}
	NEXTCH;
}
