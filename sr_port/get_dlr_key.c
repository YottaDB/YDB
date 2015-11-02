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
#include "stringpool.h"

GBLREF io_pair		io_curr_device;

void get_dlr_key(mval *v)
{
	mstr x;
	char buff[128], *cp, *cend;

	x.len = SIZEOF(buff);
	x.addr = buff;
	(io_curr_device.in->disp_ptr->dlr_key)(&x);
	v->mvtype = MV_STR;
	v->str.addr = cp = x.addr;
	cend = cp + x.len;
	for ( ; cp < cend && *cp ; cp++)
		;
	v->str.len = INTCAST(cp - v->str.addr);
	s2pool(&v->str);
}
