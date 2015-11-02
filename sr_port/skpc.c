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

/*
 *	SKPC.C - Emulate the VAX skpc instruction
 *
 * This function skips over any leading characters in a string
 * (described by the `string' and `length' parameters) that match
 * a given character (the `c' parameter).  It returns the number
 * of characters remaining in the string after the matching
 * characters that have been skipped.
 *
 */
#include "mdef.h"

int	skpc (char c, int length, char *string)
{
	while (length > 0)
	{
		if (c != *string)
			return length;
		--length;
		++string;
	}
	return 0;
}
