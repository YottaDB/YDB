/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "cmidef.h"

/* scan and reset the first fd
 * n - max fd (+1) to look at
 *
 * return -1 (if no bits set) or fd
 */
int cmj_firstone(fd_set *s, int n)
{
	int i;

	assertpro(FD_SETSIZE >= n);
	for (i = 0; i < n; i++) {
		if (FD_ISSET(i, s)) {
			FD_CLR(i, s);
			return i;
		}
	}
	return -1;
}
