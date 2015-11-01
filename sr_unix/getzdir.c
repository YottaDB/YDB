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
#include "getzdir.h"

#define MAX_DIR_LEN	512

GBLDEF mval dollar_zdir;
extern int errno;
static char data_buffer[MAX_DIR_LEN + 1];

void getzdir(void)
{
	uint4 length;
	char *getcwd_res;

	if (GETCWD(data_buffer, MAX_DIR_LEN, getcwd_res) == 0)
		rts_error(VARLSTCNT(1) errno);
	else
	{	length = strlen(data_buffer);
		data_buffer[length] = '/';
		length++;
	}

	dollar_zdir.mvtype = MV_STR;
	dollar_zdir.str.addr = data_buffer;
	dollar_zdir.str.len = length;
	return;
}
