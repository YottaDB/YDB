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
#include "cliif.h"

LITREF unsigned char  lower_to_upper_table[];

bool cli_get_str_ele_upper(char *inbuff, char *dst, unsigned short *dst_len)
{
	*dst_len = 0;
	while (*inbuff && ',' != *inbuff)
	{
		*dst++ = lower_to_upper_table[*inbuff++];
		(*dst_len)++;
	}
	*dst = 0;
	return (*dst_len ? TRUE : FALSE);
}
