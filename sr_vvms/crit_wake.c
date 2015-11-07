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
#include <ssdef.h>
#include <prvdef.h>


int	crit_wake(uint4 *pid)
{
	uint4	status, prvprv[2],
			prvadr[2] = { PRV$M_WORLD, 0 };


	if ((status = sys$setprv(TRUE, prvadr, FALSE, prvprv)) == SS$_NORMAL)
	{
		status = sys$wake(pid, NULL);

		if ((prvprv[0] & PRV$M_WORLD) == 0)
			(void)sys$setprv(FALSE, prvadr, FALSE, NULL);
	}

	return (int)status;
}
