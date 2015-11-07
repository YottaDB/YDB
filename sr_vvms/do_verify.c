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

/* do_verify.c VMS - call user-supplied collation type and version
 *                   verification routine.
 */

#include "mdef.h"
#include "collseq.h"

int4 do_verify(csp, type, ver)
	collseq *csp;
	unsigned char type, ver;
{
	return (*csp->verify)(type,ver);
}
