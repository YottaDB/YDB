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

#include "gtm_string.h"

#include "longcpy.h"

#define MAXLEN 65535

void longcpy(uchar_ptr_t a, uchar_ptr_t b, int4 len)
{
	while (len > MAXLEN)
	{
		memcpy(a, b, MAXLEN);
		a += MAXLEN;
		b += MAXLEN;
		len -= MAXLEN;
	}
	memcpy(a,b, len);
	return;
}
