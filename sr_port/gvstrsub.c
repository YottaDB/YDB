/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "collseq.h"
#include "gdsfhead.h"
#include "patcode.h"
#include "do_xform.h"
#include "gvstrsub.h"

GBLREF	gv_namehead	*gv_target;
GBLREF	uint4		*pattern_typemask;
GBLREF	bool		transform;

unsigned char *gvstrsub(unsigned char *src, unsigned char *target)
{
	int length, n;
	char buf[256], buf1[256], *ptr;
	unsigned char *ch1;
	unsigned char x;
	mstr		mstr_x;
	mstr		mstr_tmp;
	unsigned char order1,  order2;
	bool dollar_c,  was_dollar_c,  string;

	n = 0;
	ptr = buf;
	ch1 = src;
	while (*ch1)
	{
		n++;
                if (*ch1 == 1)
                {
                        ch1++;
                        *ptr++ = *ch1 - 1;
                }
                else
                        *ptr++ = *ch1;
                ch1++;
	}

	if (transform && gv_target && gv_target->collseq)
	{
		mstr_x.len = n;
		mstr_x.addr = buf;
		mstr_tmp.len = 256;
		mstr_tmp.addr = buf1;
		do_xform(gv_target->collseq->xback, &mstr_x, &mstr_tmp, &length);
		n = length;
		ch1 = (unsigned char *)buf1;
	}
	else
	{	ch1 = (unsigned char *)buf;
	}

	if (n == 0)	/* fake open quotation mark to balance close quotation at end of loop */
			/* (n==0) => loop will not be executed and, hence, won't generate open quotation mark */
		*target++ = '"';

	string = was_dollar_c = dollar_c = FALSE;
	for (; n > 0; n--)
	{
		x = *ch1;
		ch1++;
		if (!(dollar_c = ((pattern_typemask[ x ] & PATM_C) != 0)))
		{
			if (was_dollar_c)
			{
				*target++ = ')';
				*target++ = '_';
				was_dollar_c = FALSE;
			}
			if (!string)
			{	string = TRUE;
				*target++ = '"';
			}
			*target++ = x;
			if (x == '\"')
				*target++ = x;
		}

		if (dollar_c)
		{
			if (!was_dollar_c)
			{
				if (string)
				{
					*target++ = '"';
					*target++ = '_';
				}
				*target++ = '$';
				*target++ = 'C';
				*target++ = '(';
			}
			else
				*target++ = ',';

			if (order1 = x/100)
				*target++ = order1 + 48;
			if ((order2 = (x - (order1 * 100))/10) || order1)
				*target++ = order2 + 48;
			*target++ = x - (order1 * 100) - (order2 * 10) + 48;
			was_dollar_c = TRUE;
			string = FALSE;
		}
	}

	if (was_dollar_c)
		*target++ = ')';	/* close up "$C(..." at end of string by making it "$C(...)" */
	else
		*target++ = '"';	/* close up "\"..." at end of string by making it "\"...\"" */

	return target;
}












