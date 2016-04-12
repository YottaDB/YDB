/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"

void wcs_sleep(unsigned int sleepfactor)
{
	int	slpfctr;

	/* wcs_sleep provides a layer over hiber_start that produces a varying to a maximum sleep time
	 * it is intended to be used in as part of a database spin wait
	 * where the argument is a loop counter
	 * if the counter starts at 0, the invocation would typically be:
	 * if (count) wcs_sleep(count);
	 */
	slpfctr = (sleepfactor > MAXSLPTIME) ? MAXSLPTIME : sleepfactor;
	SHORT_SLEEP(slpfctr);
	return;
}
