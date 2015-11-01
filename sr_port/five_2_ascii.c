/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "five_2_ascii.h"

unsigned char *five_2_ascii(unsigned short *inval, unsigned char *cp)
{
	int4 	val;

	val = *inval;
	*cp++ = (val >> 11) + '@';
	*cp++ = ((val >> 6) & 0x1f) + '@';
	*cp++ = ((val >> 1) & 0x1f) + '@';
	return cp;
}
