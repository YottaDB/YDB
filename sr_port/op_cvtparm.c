/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cvtparm.h"
#include "op.h"

void op_cvtparm(int iocode, mval *src, mval *dst)
{
	int status;
	if ((status = cvtparm(iocode, src, dst)) == 0)
		return;
	rts_error(VARLSTCNT(1) status);
}
