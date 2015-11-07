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
#include "io.h"

#define TRL_OFF 4

GBLREF io_pair		io_curr_device;		/* current device	*/

/* This module checks whether standard and out are the same.
In VMS, it gets the input device from the previously established GT.M structure and the output device from its caller.
In UNIX, it ignores its arguments and gets the devices from the system designators */
bool   same_device_check(mstr tname, char buf[MAX_TRANS_NAME_LEN])
{
	if (io_curr_device.in->type == io_type(&tname))
	{
		if (io_curr_device.in->trans_name->len == tname.len - TRL_OFF &&
		    !memcmp(&io_curr_device.in->trans_name->dollar_io[0],
		    &buf[TRL_OFF], tname.len - TRL_OFF))
			return TRUE;
	}
	return FALSE;
}
