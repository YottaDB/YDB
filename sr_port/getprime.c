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

int4 getprime (int4 n)
/* Returns first prime # >= n */
{
	int m, p;

	for (m = n | 1 ; ; m += 2)
	{
		for (p = 3 ; ; p += 2)
		{
			if (p * p > m)
				return m;
			if (((m / p) * p) == m)
				break;
		}
	}
}
