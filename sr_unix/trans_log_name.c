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

#include <string.h>
#include "gtm_stdlib.h"

#include "io.h"
#include "iosp.h"
#include "trans_log_name.h"

#define MAX_NUMBER_FILENAMES	256*MAX_TRANS_NAME_LEN

static readonly char alnum[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

uint4 trans_log_name(mstr *log, mstr *trans, char *buffer)
{
	char	*s_start, *s_ptr, *tran_buff, *b_ptr;
	int	i;
	uint4	ret;
	char	temp[128], buff[MAX_NUMBER_FILENAMES];

	b_ptr = buffer;
	ret = SS_NOLOGNAM;
	memcpy(buff, log->addr, log->len);
	buff[log->len] = 0;
	for (s_start = s_ptr = buff, i = 0; i < log->len; )
	{
		if (*s_ptr == '$')
		{
			memcpy(b_ptr, s_start, s_ptr - s_start);
			b_ptr += (s_ptr - s_start);
			s_start = s_ptr++;
			while (alnum[*s_ptr])
				s_ptr++;
			memcpy(temp, s_start + 1, s_ptr - s_start - 1);
			temp[s_ptr - s_start - 1] = 0;
			if (tran_buff = GETENV(temp))
			{	ret = SS_NORMAL;
				while (*b_ptr++ = *tran_buff++)
					;
				b_ptr--;
			} else
			{	memcpy(b_ptr, s_start, s_ptr - s_start);
				b_ptr += (s_ptr - s_start);
			}
			i += s_ptr - s_start;
			s_start = s_ptr;
		}else
		{
			s_ptr++;
			i++;
		}
	}
	memcpy(b_ptr, s_start, s_ptr - s_start);
	trans->addr = buffer;
	trans->len = b_ptr - buffer + s_ptr - s_start;
	return ret;
}
