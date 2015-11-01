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

#include "gtm_string.h"
#include "min_max.h"
#include "mstrcmp.h"

int	mstrcmp(mstr *s1, mstr *s2)
{
	int rc;

	rc = memcmp(s1->addr, s2->addr, MIN(s1->len, s2->len));
	if (0 > rc)
		return -1;
	else if (0 < rc)
		return 1;
	/* value tested is equal so far */
	if (s1->len < s2->len)
		return -1;
	else if (s2->len < s1->len)
		return 1;
	return 0;
}
