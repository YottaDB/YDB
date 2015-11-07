/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <descrip.h>

#include "gdsroot.h"


void global_name (prefix, fil, buff)
unsigned char	prefix[4];
gds_file_id	*fil;
unsigned char	*buff;
{
	unsigned char *cp;
	int i, j, k, n;

	/* The routine MU_SEC_CLEAN decodes this algorithm.  Keep the two routines in tandem.  */

	cp = buff + 1;
	memcpy(cp, prefix, SIZEOF(prefix));
	cp += SIZEOF(prefix);
	i = fil->dvi[0];
	memcpy(cp, &fil->dvi[1], i);
	cp += i;
	*cp++ = '$';
	for (i = 0 ; i < SIZEOF(fil->fid) / SIZEOF(fil->fid[0]) ; i++)
	{
		for (k = fil->fid[i], j = 0 ; j < SIZEOF(fil->fid[0]) * 2; j++, k = k >> 4)
		{
			n = k & 0xf;
			*cp++ = n + (n < 10 ? 48 : 55);
		}
	}
	assert(cp - buff <= GLO_NAME_MAXLEN);
	*buff = cp - buff - 1;
	return;
}
