/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_ctype.h"

LITREF unsigned char lower_to_upper_table[];

unsigned int asc_hex2i(uchar_ptr_t p, int len)
{
	uchar_ptr_t	c;
	unsigned char	ch;
	int		ret;

	ret = 0;
	for (c = p + len; c > p; p++)
	{
		if (('0' <= *p) && ('9' >= *p))
			ret = (ret << 4) + (*p - '0');
		else
		{
			ch = TOUPPER(*p);
			if (('A' <= ch) && ('F' >= ch))
				ret = (ret << 4) + ch - 'A' + 10;
			else
				return (unsigned int)-1;
		}
	}
	return ret;
}

#ifndef VMS
/* Routine identical to asc_hex2i() but with 8 byte accumulator and return type */
gtm_uint64_t  asc_hex2l(uchar_ptr_t p, int len)
{
	uchar_ptr_t	c;
	unsigned char	ch;
	gtm_uint64_t	ret;

	ret = 0;
	for (c = p + len; c > p; p++)
	{
		if (('0' <= *p) && ('9' >= *p))
			ret = (ret << 4) + (*p - '0');
		else
		{
			ch = TOUPPER(*p);
			if (('A' <= ch) && ('F' >= ch))
				ret = (ret << 4) + ch - 'A' + 10;
			else
				return (gtm_uint64_t)-1;
		}
	}
	return ret;
}
#endif
