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

/* iorm_read.c */

#include "mdef.h"
#include "io.h"

int	iorm_read(mval *v, int4 msec_timeout)
{
	return iorm_readfl(v, 0, msec_timeout); /* 0 means not fixed length */
}
