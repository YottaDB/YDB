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
#include "sgnl.h"

int sgnl_assert(unsigned int filesz, unsigned char *file, unsigned int linenum)
{
	error_def(ERR_ASSERT);

	return rts_error(VARLSTCNT(5) ERR_ASSERT, 3, filesz, file, linenum);
}
