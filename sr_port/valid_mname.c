/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

	if ((0 < targ->len) && (targ->len <= MAX_MIDENT_LEN) && VALID_MNAME_FCHAR(*src))
	{	/* we currently truncate the name at length = MAX_MIDENT_LEN */
		/* a comment in GTM-5284 suggests we should parse the whole name in order to give a more helpful error */
		for (; ++src < src_top;)
		{
			if (!VALID_MNAME_NFCHAR(*src))
				break;
		}
		return (src == src_top); /* Was it a valid M identifier? */
	}
	return FALSE;
}
