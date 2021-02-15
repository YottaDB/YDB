/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "iott_wrterr.h"

GBLREF int	iott_write_error;

error_def(ERR_TERMWRITE);

void iott_wrterr(void)
{
	int	status;

	status = iott_write_error;
	iott_write_error = 0;
	RTS_ERROR_ABT(VARLSTCNT(3) ERR_TERMWRITE, 0, status);
}
