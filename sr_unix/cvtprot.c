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
#include "cvtprot.h"

#define READ 4
#define WRITE 2
#define EXECUTE 1

int cvtprot(char *cp, short cnt)
{
	int	mask;

	/* a protection mask consists of 3 bits per access, R(ead), W(rite), and X(excute)
	*/

	mask = 0;
	for (;cnt > 0 ; cnt--, cp++)
	{
		switch (*cp)
		{
			case 'R':
			case 'r':
				mask |= READ;
				break;
			case 'W':
			case 'w':
				mask |= WRITE;
				break;
			case 'X':
			case 'x':
				mask |= EXECUTE;
				break;
			default:
				return -1;
		}
	}
	return mask;
}

