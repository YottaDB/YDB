/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "toktyp.h"
#include "valid_mname.h"

boolean_t valid_mname(mstr *targ)
{
	char	*src, *src_top;

	src = targ->addr;
	src_top = targ->addr + targ->len;

	if (0 < targ->len && targ->len <= MAX_MIDENT_LEN && VALID_MNAME_FCHAR(*src))
	{
		for ( ; ++src < src_top; )
		{
			if (!VALID_MNAME_NFCHAR(*src))
				break;
		}
		return (src == src_top); /* Was it a valid M identifier? */
			/* See D9E02-002422 for an issue here. Also we are supposed to trucate the name at lenght = 31 */
	}
	return FALSE;
}
