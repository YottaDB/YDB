/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "io.h"

/* This module checks whether standard in and standard out are the same.
 * In VMS, it gets the input device from the previously established GT.M structure and the output device from its caller.
 * In UNIX, it ignores its arguments and gets the devices from the system designators
 * st_mode includes permissions so just check file type
 */

bool	same_device_check (mstr tname, char *buf)
{
	int		fstat_res;
	struct stat	outbuf1, outbuf2;

	FSTAT_FILE(0, &outbuf1, fstat_res);
	FSTAT_FILE(1, &outbuf2, fstat_res);
	return ((S_IFMT & outbuf1.st_mode) == (S_IFMT & outbuf2.st_mode));
}
