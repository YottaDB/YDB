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
#include "gtm_caseconv.h"

LITREF unsigned char upper_to_lower_table[];

void upper_to_lower (unsigned char *d, unsigned char *s, int4 len)
{
	unsigned char	*d_top;

	d_top = d + len;
	for ( ; d < d_top; )
	{
		*d++ = upper_to_lower_table[*s++];
	}
}
