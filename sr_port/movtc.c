/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "movtc.h"

void	movtc(int length, unsigned char *inbuf, const unsigned char table[], unsigned char *outbuf)
{
	while (length > 0)
	{
		*outbuf++ = table[(int)*inbuf++];
		--length;
	}
}
