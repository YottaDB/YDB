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
#include "iott_wrterr.h"

GBLREF int	iott_write_error;

void iott_wrterr(void)
{
	int	status;
	error_def(ERR_TERMWRITE);

	status = iott_write_error;
	iott_write_error = 0;
	rts_error(VARLSTCNT(3) ERR_TERMWRITE, 0, status);
}
