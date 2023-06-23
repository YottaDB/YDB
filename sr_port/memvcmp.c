/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

int memvcmp(void *a, int a_len, void *b, int b_len)
{
	int	retval;

	MEMVCMP(a, a_len, b, b_len, retval);
	return retval;
}
