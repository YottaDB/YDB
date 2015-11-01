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
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"

GBLREF bool zdefactive;

void gvcmx_put(mval *v)
{
	if (zdefactive)
		gvcmz_bunch(v);
	else
		gvcmz_doop(CMMS_Q_PUT, CMMS_R_PUT, v);
}
