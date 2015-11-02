/****************************************************************
 *								*
 *	Copyright 2002, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include <rtnhdr.h>

/* Format the given routine, label and offset into user presentable label+offset^routine string,
 * return end of formatted output + 1. Does not check for buffer overflow, it's caller's
 * responsibility to pass buffer with enough space */
char *rtnlaboff2entryref(char *entryref_buff, mident *rtn, mident *lab, int offset)
{
	char	*ptr, *from, *top;

	ptr = entryref_buff;
	memcpy(ptr, lab->addr, lab->len);
	ptr += lab->len;
	if (0 != offset)
	{
		*ptr++ = '+';
		ptr = (char *)i2asc((uchar_ptr_t)ptr, offset);
	}
	*ptr++ = '^';
	memcpy(ptr, rtn->addr, rtn->len);
	ptr += rtn->len;
	return ptr;
}
