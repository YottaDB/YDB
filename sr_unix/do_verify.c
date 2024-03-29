/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* do_verify.c UNIX - call user-supplied collation type and version
 *                    verification routine.
 */

#include "mdef.h"
#include "collseq.h"

int4 do_verify(collseq *csp, unsigned char type, unsigned char ver)
{
	assert(NULL != csp);
	return (0 == (*csp->verify)(type,ver));
}
