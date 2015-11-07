/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"

/* Searches a global directory "gblnames" array for which entry an input "global-name" falls in */
gd_gblname *gv_srch_gblname(gd_addr *addr, char *name, int name_len)
{
	int		res;
	int		low, high, mid;
	gd_gblname	*gname_start, *gname;

	assert(addr->n_gblnames);	/* caller should have taken care of this */
	/* At all times in the loop, "low" corresponds to the smallest possible value for "gname"
	 * and "high" corresponds to one more than the highest possible value for "gname".
	 */
	low = 0;
	high = addr->n_gblnames;
	gname_start = &addr->gblnames[low];
	do
	{
		if (low == high)
			break;
		assert(low < high);
		mid = (low + high) / 2;
		assert(low <= mid);
		assert(mid < high);
		gname = &gname_start[mid];
		res = memcmp(name, gname->gblname, name_len);
		if (0 > res)
			high = mid;
		else if (0 < res)
			low = mid + 1;
		else if ('\0' == gname->gblname[name_len])
			return gname;
		else
			high = mid;
	} while (TRUE);
	return NULL;
}
