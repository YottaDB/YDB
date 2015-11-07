/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

int exttime(uint4 short_time, char *buffer, int extract_len)
{
	char		*p;
	jnl_proc_time	VMS_time;
	uint4		days, h_seconds;

	JNL_WHOLE_FROM_SHORT_TIME(VMS_time, short_time);
	/* Convert the VMS time to the number of days since the system
	   zero date, and the number of hundredths of a second since midnight */
	lib$day(&days, &VMS_time, &h_seconds);
	/* Convert days and h_seconds to $Horolog format */
	p = i2asc((unsigned char *)(buffer + extract_len), days + DAYS);
	*p++ = ',';
	p = i2asc(p, h_seconds / CENTISECONDS);
	*p++ = '\\';
	return p - buffer;
}
