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
#include "io.h"
#include "iottdef.h"
#include "stringpool.h"

GBLREF io_pair 		io_curr_device;
GBLREF spdesc 		stringpool;

int iott_read( mval *v, int4 t)
{
	int4		length;

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	length = ((d_tt_struct*)(io_curr_device.in->dev_sp))->in_buf_sz;
	ENSURE_STP_FREE_SPACE(length + ESC_LEN);
	v->str.addr = stringpool.free;
	return iott_readfl(v,length,t);
}
