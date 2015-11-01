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

/* Compare two character strings using "0" as a filler
	return 0 iff they are equal
	return < 0 if a < b and > 0 if a > b	*/

#include "mdef.h"
#include "gtm_string.h"
#include "mmemory.h"

int memvcmp(void *a,
	    int a_len,
	    void *b,
	    int b_len)
{
	int	i;

	if (0 == (i = memcmp(a,b,a_len > b_len ? b_len : a_len)))
	{	if (b_len > a_len)
			return -1;
		else if (a_len > b_len)
			return 1;
	}
	return i;
}
