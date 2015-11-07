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
#include <syidef.h>
#include "get_page_size.h"

GBLDEF int4 gtm_os_page_size;

void get_page_size(void)
{
	int4 status;

	status = lib$getsyi(&SYI$_PAGE_SIZE, &gtm_os_page_size, 0, 0, 0, 0);
	if ((status & 1) == 0)
		rts_error(status);
	return;
}
