/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_caseconv.h"
#include "gtm_string.h"

LITREF unsigned char lower_to_upper_table[];

void str_to_title (unsigned char *d, unsigned char *s, int4 len)
{
	boolean_t       up;
	unsigned        char *c, *top;

	assert(0 <= len);
	if (0 >= len)
		return;
	upper_to_lower(d, s, len);
	c = d;
	for (top = c + len, up = TRUE; c < top; c++)
	{
		if (' ' == *c)
		{
			up = TRUE;
			continue;
		}
		if (up && (97 <= *c) && (122 >= *c))
		{
			*c = lower_to_upper_table[*c];
			up = FALSE;
		}
	}
}
