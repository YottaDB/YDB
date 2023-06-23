/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"
#include "util.h"
#include "dse_puttime.h"

#define TIME_SIZE	(2 + 1 + 2 + 1 + 2 + 1 + 2)	/* hh:mm:ss:dd */
/*	display uint8 *, representing a time value in nanoseconds */
void dse_puttime(uint8 *time, char *c, bool flush)
{
	int time_ms;
	char 	outbuf[TIME_SIZE * 4];		/* Leave room for unexpected values */

	time_ms = *time / NANOSECS_IN_MSEC;
	SNPRINTF(outbuf, TIME_SIZE * 4, "%2.2d:%2.2d:%2.2d:%2.2d", time_ms / 3600000,
		(time_ms % 3600000) / 60000, (time_ms % 60000) / 1000,
		(time_ms % 1000) / 10);
	util_out_print(c,flush,TIME_SIZE,outbuf);
	return;
}
