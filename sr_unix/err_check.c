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
#include "merror.h"

LITREF	err_ctl *master_msg[];
LITREF	err_ctl merrors_ctl;

err_ctl *err_check(err)
int err;
{
	err_ctl	*fac;
	int i,j;

        if (0 > err)
                return 0;

	if ((err & FACMASK(merrors_ctl.facnum)) && ((j = MSGMASK(err, merrors_ctl.facnum)) <= merrors_ctl.msg_cnt))
		return &merrors_ctl;

	for (i = 0; master_msg[i]; i++)
	{
		fac = master_msg[i];
		if ((err & FACMASK(fac->facnum)) && ((j = MSGMASK(err, fac->facnum)) <= fac->msg_cnt))
			return fac;
	}
	return 0;
}
