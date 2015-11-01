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

#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"

GBLREF spdesc stringpool;

oprtype put_str(char *pt,int n)
{
	mval p;

	if (stringpool.top - stringpool.free < n)
		stp_gcol(n);
	memcpy(stringpool.free, pt, n);
	p.mvtype = MV_STR;
	p.str.len = n;
	p.str.addr = (char *) stringpool.free;
	stringpool.free += n;
	s2n(&p);
	return put_lit(&p);
}
