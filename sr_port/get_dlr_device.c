/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

void get_dlr_device(mval *v)
{
	mstr	x;
	char 	*cp, *cend;

	x.len = DD_BUFLEN;	/* Default length, if dollar.device[] array is used. */
	x.addr = NULL;
	(io_curr_device.in->disp_ptr->dlr_device)(&x);
	assert((0 >= x.len) || (NULL != x.addr));
	v->mvtype = MV_STR;
	v->str.addr = cp = x.addr;
	cend = cp + x.len;
	for ( ; cp < cend && *cp ; cp++)
		;
	v->str.len = INTCAST(cp - v->str.addr);
	s2pool(&v->str);
}
