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

#include "gdsroot.h"		/* needed for gdsfhead.h */
#include "gtm_facility.h"	/* needed for gdsfhead.h */
#include "fileinfo.h"		/* needed for gdsfhead.h */
#include "gdsbt.h"		/* needed for gdsfhead.h */
#include "gdsfhead.h"
#include "stringpool.h"
#include "format_targ_key.h"
#include "get_reference.h"	/* for get_reference() prototype */

GBLREF gv_key	*gv_currkey;
GBLREF spdesc	stringpool;
GBLREF mstr	extnam_str;

void get_reference(mval *var)
{
	char	*end, *start;
	char	extnamdelim[] = "^|\"\"|";
	char	*extnamsrc, *extnamtop;
	int	maxlen;

	/* you need to return a double-quote for every single-quote. assume worst case. */
	maxlen = MAX_ZWR_KEY_SZ + (!extnam_str.len ? 0 : ((extnam_str.len * 2) + SIZEOF(extnamdelim)));
	ENSURE_STP_FREE_SPACE(maxlen);
	var->mvtype = MV_STR;
	start = var->str.addr = (char *)stringpool.free;
	var->str.len = 0;
	if (gv_currkey && gv_currkey->end)
	{
		if (extnam_str.len)
		{
			*start++ = extnamdelim[0];
			*start++ = extnamdelim[1];
			*start++ = extnamdelim[2];
			extnamsrc = &extnam_str.addr[0];
			extnamtop = extnamsrc + extnam_str.len;
			for ( ; extnamsrc < extnamtop; )
			{
				*start++ = *extnamsrc;
				if ('"' == *extnamsrc++)	/* caution : pointer increment side-effect */
					*start++ = '"';
			}
			*start++ = extnamdelim[3];
		}
		end = (char *)format_targ_key((unsigned char *)start, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
		if (extnam_str.len)
			/* Note: the next vertical bar overwrites the caret that
			 * was part of he original name of the global variable
			 */
			*start = extnamdelim[4];
		var->str.len = INTCAST(end - var->str.addr);
		stringpool.free += var->str.len;
	}
}
