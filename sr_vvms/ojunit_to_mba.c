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

unsigned short ojunit_to_mba (targ, n)
char		*targ;
uint4	n;
{
	char		buf[16];
	unsigned short	i, len;

	i = 0;
	do {
		buf[i++] = '0' + n % 10;
		n = n / 10;
	} while (n != 0);
	len = i + 5;

	*targ++ = '_';
	*targ++ = 'M';
	*targ++ = 'B';
	*targ++ = 'A';
	do {
		*targ++ = buf[--i];
	} while (i > 0);
	*targ++ = ':';

	return len;
}
