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

/* Although similar functions can be found in i2asc() and i2mval(),
 * this function works more efficiently which is important
 * because this function will be called a lot during a mupip
 * extract which is very time-consuming for big database.
 */

int i2a(unsigned char *des, int *des_len, int num)
{
        int n;

        if(0 != (n = (num/100)%10))
	        *(des + ((*des_len)++)) = n + '0';
	if(0 != (n = (num/10)%10))
	        *(des + ((*des_len)++)) = n + '0';
	n = num%10;
	*(des + ((*des_len)++)) = n + '0';

	return 0;
}
