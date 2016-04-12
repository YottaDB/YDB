/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "iousdef.h"
#include "stringpool.h"

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

int ious_read(mval *v, int4 t)
{
	v->str.len = 0;
	v->str.addr = (char *)0;
	return TRUE;
}
