/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "rtnhdr.h"

char *rtnlaboff2entryref(char *entryref_buff, mstr *rtn, mstr *lab, int offset)
{ /* format given routine, label and offset into user presentable label+offset^routine string, return end of formatted output + 1 */
  /* does not check for buffer overflow, caller's responsibility to pass buffer with enough space */
	char	*entryref_p, *from, *top;

	entryref_p = entryref_buff;
	for (from = lab->addr, top = lab->addr + lab->len; from < top && '\0' != *from; )
		*entryref_p++ = *from++;
	if (0 != offset)
	{
		*entryref_p++ = '+';
		entryref_p = (char *)i2asc((uchar_ptr_t)entryref_p, offset);
	}
	*entryref_p++ = '^';
	for (from = rtn->addr, top = rtn->addr + rtn->len; from < top && '\0' != *from; )
		*entryref_p++ = *from++;
	return entryref_p;
}
