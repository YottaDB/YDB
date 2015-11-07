/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

void get_dlr_zkey(mval *v)
{
#	ifdef UNIX
	mstr x;
	char buff[128], *cp, *cend;

	if (rm == io_curr_device.in->type)
	{
		x.len = SIZEOF(buff);
		x.addr = buff;
		(io_curr_device.in->disp_ptr->dlr_zkey)(&x);
		v->mvtype = MV_STR;
		v->str.addr = cp = x.addr;
		cend = cp + x.len;
		for ( ; cp < cend && *cp ; cp++)
			;
		v->str.len = INTCAST(cp - v->str.addr);
		s2pool(&v->str);
	} else
#	endif
	{
		(io_curr_device.in->disp_ptr->dlr_zkey)(&v->str);
		v->mvtype = MV_STR;
	}
}
