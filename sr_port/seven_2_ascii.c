/****************************************************************
 *								*
 *	Copyright 2002, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "seven_2_ascii.h"

int seven_2_ascii(unsigned char *inpt, unsigned char *outp)
{
	unsigned char	*p1, *p2;
	int		in_val;

	p1 = inpt;
	p2 = outp;
	do
	{
		in_val = *p1++;
		*p2++ = (in_val >> 1);
	}  while (in_val & 1); /* end do */
	return (int4)(p2 - outp); /* length of output */
}
