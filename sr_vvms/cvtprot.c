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
#include <xab.h>
#include "cvtprot.h"

int cvtprot(char *cp, short cnt)
{
	int	mask;

	/* a protection mask consists of 4 bits, R(ead), W(rite), E(xecute), and D(elete).
	   to deny access, the bit is one; to grant access, the
	   bit is set to zero.
	*/
	mask = 0;
	for (;cnt > 0 ; cnt--, cp++)
	{
		switch (*cp)
		{
			case 'R':
			case 'r':
				mask |= XAB$M_NOREAD;
				break;
			case 'W':
			case 'w':
				mask |= XAB$M_NOWRITE;
				break;
			case 'E':
			case 'e':
				mask |= XAB$M_NOEXE;
				break;
			case 'D':
			case 'd':
				mask |= XAB$M_NODEL;
				break;
			default:
				return -1;
		}
	}
	return mask;
}

