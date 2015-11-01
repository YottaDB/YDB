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

LITREF unsigned char lower_to_upper_table[];

void lower_to_upper(uchar_ptr_t d, uchar_ptr_t s, int4 len)
{
	uchar_ptr_t d_top;

	d_top = d + len;
	for ( ; d < d_top; )
	{	*d++ = lower_to_upper_table[*s++];
	}
}
