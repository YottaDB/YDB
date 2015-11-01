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

#include "gtm_string.h"

#include "io.h"
#include  "gtm_stat.h"
#include  "gtm_stdio.h"
#include  "eintr_wrappers.h"

bool io_is_rm(mstr *name)
{
	int	stat_res;
    	struct  stat  outbuf;
	char buffer[BUFSIZ];

   	assert(BUFSIZ > name->len);
	memcpy(buffer, name->addr, name->len);
	buffer[name->len] = '\0';
	STAT_FILE(buffer, &outbuf, stat_res);
	return (0 == stat_res && S_ISREG(outbuf.st_mode));
}
