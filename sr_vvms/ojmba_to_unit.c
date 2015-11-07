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

uint4 ojmba_to_unit (src)
char	*src;
{
	uint4	n;

	n = 0;
	assert (*src == 'M');
	src++;
	assert (*src == 'B');
	src++;
	assert (*src == 'A');
	src++;
	while (('0' <= *src) && (*src <= '9'))
	{
		n *= 10;
		n += (*src - '0');
		src++;
	}

	return n;
}
