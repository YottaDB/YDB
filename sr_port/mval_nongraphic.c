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
#include "mlkdef.h"
#include "zshow.h"

void mval_nongraphic(zshow_out *output,char *cp, int len, int num)
{
	/* sub-program for mval_write() */
	mval tmpmval;
	char buff[10];	/* sufficient to hold all possible Unicode code point values */
	char *ptr;
	int n, m;

	tmpmval.mvtype = MV_STR;
	tmpmval.str.addr = cp;
	tmpmval.str.len = len;
	zshow_output(output,&tmpmval.str);
	for (ptr = buff + SIZEOF(buff) , n = num, m = SIZEOF(buff) ; m > 0 ; m--)
	{
		*--ptr = (n % 10) + '0';
		n /= 10;
		if (!n)
			break;
	}
	tmpmval.str.addr = ptr;
	tmpmval.str.len = INTCAST(buff - ptr + SIZEOF(buff));
	zshow_output(output,&tmpmval.str);
	return;
}
