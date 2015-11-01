/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
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

bool cli_get_str_ele(char *inbuff, char *dst, unsigned short *dst_len, boolean_t upper_case)
{
	*dst_len = 0;
	while (*inbuff && ',' != *inbuff)
	{
		if (upper_case)
			*dst++ = lower_to_upper_table[*inbuff++];
		else
			*dst++ = *inbuff++;
		(*dst_len)++;
	}
	*dst = 0;
	return (*dst_len ? TRUE : FALSE);
}
