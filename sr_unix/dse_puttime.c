/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
/*	display uint4 *, representing a time value in millisec */
void dse_puttime(int_ptr_t time, char *c, bool flush)
{
	char 	outbuf[TIME_SIZE * 4];		/* Leave room for unexpected values */

	SPRINTF(outbuf,"%2.2d:%2.2d:%2.2d:%2.2d", *time / 3600000,
		(*time % 3600000) / 60000, (*time % 60000) / 1000,
		(*time % 1000) / 10);
	util_out_print(c,flush,TIME_SIZE,outbuf);
	return;
}
