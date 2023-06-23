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

/* This module determines whether a vms device is NONLOCAL and therefore a network device, generally sys$net.
In UNIX, it always returns false. */
bool io_is_sn(mstr *tn)
{
	return FALSE;
}
