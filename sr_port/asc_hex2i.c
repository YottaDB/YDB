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

unsigned int asc_hex2i(p,len)
char *p;
int  len;
{
	char	*c;
	int	ret;

	ret = 0;
	for (c = p + len; c > p; p++)
	{
		if (*p >= '0' && *p <= '9')
			ret = ret * 16 + *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			ret = ret * 16 + *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			ret = ret * 16 + *p - 'A' + 10;
		else
			return (uint4)-1;
	}
	return ret;
}
