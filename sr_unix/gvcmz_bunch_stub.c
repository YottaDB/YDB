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

/*** STUB FILE ***/

#include "mdef.h"
#include "cmidef.h"
#include "gvcmz.h"

GBLDEF bool zdefactive;
GBLDEF unsigned short zdefbufsiz;

void gvcmz_bunch(mval *v)
{
	error_def(ERR_UNIMPLOP);
	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
}
