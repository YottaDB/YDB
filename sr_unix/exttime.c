/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "dollarh.h"

int exttime (uint4 time, char *buffer, int extract_len)
{
	unsigned char		*ptr;
	uint4			days;
	time_t			seconds;

	dollarh((time_t)time, &days, &seconds);	/* Convert time to $Horolog format */
	ptr = i2asc((unsigned char *)(buffer + extract_len), days);
	*ptr++ = ',';
	ptr = i2asc(ptr, (uint4)seconds);
	*ptr++ = '\\';
/* The use of this fn is only once, that too only as offset.. */
	return (int)((char *)ptr - buffer);
}
