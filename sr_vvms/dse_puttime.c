/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <descrip.h>
#include "util.h"
#include "dse_puttime.h"

void dse_puttime(int_ptr_t time, char *c, bool flush)
{	unsigned char outbuf[12];
	short unsigned	timelen;
	$DESCRIPTOR(time_desc,outbuf);

	memset(outbuf, 0, SIZEOF(outbuf));
	sys$asctim(&timelen, &time_desc, time, 1);
	util_out_print(c,flush,timelen,outbuf);
	return;
}
