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

unsigned short ojhex_to_str (s, t)
uint4	s;
char		*t;
{
	char		buf[8];
	unsigned short	i, d, len;

	i = 0;
	do {
		d = s % 0x10;
		s = s / 0x10;
		buf[i++] = d + ((d <= 9) ? '0' : 'A' - 0xA);
	} while (s > 0);
	len = i;
	do {
		*t++ = buf[--i];
	} while (i > 0);

	return len;
}
