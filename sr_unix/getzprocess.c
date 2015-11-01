/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*** STUB FILE ***/

#include "mdef.h"
#include "getzprocess.h"

GBLDEF mval dollar_zproc;

void getzprocess(void)
{
	dollar_zproc.mvtype = MV_STR;
	dollar_zproc.str.addr = "";
	dollar_zproc.str.len = 0;
	return;
}
