/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_UNIMPLOP);

void gvcmz_bunch(mval *v)
{
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
}
