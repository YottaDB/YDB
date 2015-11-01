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
#include "longset.h"

#define MAXLEN 65535

void longset(uchar_ptr_t ptr, int len, unsigned char fill)
{
	while (len > MAXLEN)
	{
		memset(ptr, fill, MAXLEN);
		ptr += MAXLEN;
		len -= MAXLEN;
	}
	memset(ptr, fill, len);
	return;
}
