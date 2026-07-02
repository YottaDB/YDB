/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "zshow.h"

void mval_nongraphic(zshow_out *output,char *cp, int len, int num)
{
	/* sub-program for mval_write() */
	unmanaged_mstr	umstr;
	char buff[10];	/* sufficient to hold all possible UTF8 code point values */
	char *ptr;
	int n, m;

	umstr.addr = cp;
	umstr.len = len;
	zshow_output(output, &umstr);
	for (ptr = buff + SIZEOF(buff) , n = num, m = SIZEOF(buff) ; m > 0 ; m--)
	{
		*--ptr = (n % 10) + '0';
		n /= 10;
		if (!n)
			break;
	}
	umstr.addr = ptr;
	umstr.len = INTCAST(buff - ptr + SIZEOF(buff));
	zshow_output(output, &umstr);
	return;
}
