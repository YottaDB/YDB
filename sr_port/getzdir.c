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

#include "gtm_unistd.h"
#include "gtm_string.h"

#include "getzdir.h"
#include "setzdir.h"

GBLREF mval	dollar_zdir;

void getzdir(void)
{
	mval	cwd;

	setzdir(NULL, &cwd);
	if (cwd.str.len > dollar_zdir.str.len)
	{
		if (NULL != dollar_zdir.str.addr)
			free(dollar_zdir.str.addr);
		dollar_zdir.str.addr = malloc(cwd.str.len);
	}
	dollar_zdir.str.len = cwd.str.len;
	memcpy(dollar_zdir.str.addr, cwd.str.addr, cwd.str.len);
	return;
}
