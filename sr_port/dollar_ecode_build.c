/****************************************************************
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

#include "error_trap.h"
#include "stringpool.h"

GBLREF spdesc		stringpool;
GBLREF ecode_list	*dollar_ecode_list;

void dollar_ecode_build(int level, mval *val)
{
	int		len, shrink;
	char		*dest;
	ecode_list	*ecode_entry;
	error_def(ERR_ECLOSTMID);

	/* shrink = 0 means no shrink of the error code.
	 * 	  = 1 means processing the first item
	 * 	  = 2 means finished processing the first item.
	 * 	  > 2 means processing additional items.
	 */
	len = 1;
	for (ecode_entry = dollar_ecode_list;
		ecode_entry->previous;
		ecode_entry = ecode_entry->previous)
	{
		if ((level < 0) || (level == ecode_entry->level))
			len += (ecode_entry->str.len - 1);
			/* Note: one less than the length of the error code, because each individual
			 * error code contains 2 commas, and only one of them is copied to the result.
			 */
	}
	assert (0 < len);
	val->mvtype = MV_STR;
	if (1 != len)
	{
		shrink = (MAX_STRLEN < len);
		if (0 != shrink)
			len = MAX_STRLEN;
		if (len > stringpool.top - stringpool.free)
			stp_gcol(len);
		dest = val->str.addr = (char *)stringpool.free;
		for (ecode_entry = dollar_ecode_list; ecode_entry->previous; ecode_entry = ecode_entry->previous)
		{
			if ((level < 0) || (level == ecode_entry->level))
			{
				memcpy(dest, ecode_entry->str.addr, ecode_entry->str.len);
				if (2 > shrink)
				{	/* shrink not necessary, or processing the first (newest) item */
					dest += (ecode_entry->str.len - 1);
					if (shrink)
					{	/* insert place holder for lost codes */
						dest++;		/* use the trailing comma */
						*dest++ = 'Z';
						shrink = (char *)i2asc(dest, ERR_ECLOSTMID) - dest;
						dest = dest + shrink;
						len = ecode_entry->str.len + shrink + STR_LIT_LEN("Z");
						shrink = 2;	/* start saving "old" errors (there must be many) to find first */
					}
				} else
					shrink = ecode_entry->str.len;
			}
		}
		if (0 != shrink)
		{	/* all ecode values have at least a one character code and leading and ending commas */
			assert(2 < shrink);
			len += shrink;
		}
		val->str.len = len;
		stringpool.free += len;
	} else
	{
		val->str.len = 0;
		val->str.addr = NULL;
	}
}
