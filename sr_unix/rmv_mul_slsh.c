/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "rmv_mul_slsh.h"
#include "gtm_string.h"
#include "mdef.h"
/*Remove multiple slashes from the source and returns the new length*/
/*Note this function does not null terminate the new string.*/
/*This is the responsibility of the calling function*/
unsigned int rmv_mul_slsh(char *src, unsigned int src_len)
{
	char *ci, *co, *ct;
	ci = co = src;
	ct = ci + src_len - 1;
	while (ci < ct)
	{
		if (('/' != *ci) || ('/' != *(ci + 1)))
			*co++ = *ci++;
		else
			ci++;
	}
	*co = *ci;
	return (++co - src);
}
