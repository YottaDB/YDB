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
#include <dcdef.h>
#include <devdef.h>
#include <dvidef.h>
#include <descrip.h>
#include "io.h"

/* This module determines whether a vms device is NONLOCAL and therefore a network device, generally sys$net.
In UNIX, it always returns false. */
bool io_is_sn(mstr *tn)
{
	uint4	devclass;	/* device classification information */
	uint4	devtype;	/* device type information */
	int4	item_code;
	uint4	stat;

	$DESCRIPTOR(buf_desc,"");

	item_code = DVI$_DEVCLASS;

	buf_desc.dsc$a_pointer = tn->addr;
	buf_desc.dsc$w_length = tn->len;
	stat = lib$getdvi(&item_code
			 ,0
			 ,&buf_desc
			 ,&devclass
			 ,0 , 0);
	if (stat == SS$_NONLOCAL)
		return TRUE;
	else
		return FALSE;
}
