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
#include "compiler.h"

GBLREF mline *mline_tail;

oprtype put_mnxl(void)
{
	oprtype a;

	a.oprclass = MNXL_REF;
	a.oprval.mlin = mline_tail;
	return a;
}
