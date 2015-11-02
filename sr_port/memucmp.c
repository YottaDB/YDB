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
#include "mmemory.h"

int memucmp (uchar_ptr_t a, uchar_ptr_t b, uint4 siz)
{
	register uchar_ptr_t a_top;

	a_top = a + siz;
	while (a < a_top)
	{
		if (*a != *b)
		{
			if (*a < *b)
				return -1;
			else
				return 1;
		}
		a++; b++;
	}
	return 0;
}
