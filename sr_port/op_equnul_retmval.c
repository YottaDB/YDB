/****************************************************************
*								*
* Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include "op.h"

#ifdef DEBUG
GBLREF	bool	undef_inhibit;
#endif

LITREF	mval	literal_zero, literal_one;
LITREF	mval	literal_sqlnull;

void op_equnul_retmval(mval *u, mval *ret)
{
	int	mvtype;

	mvtype = u->mvtype;
	if (!MVTYPE_IS_STRING(mvtype))
	{
		if (MVTYPE_IS_NUMERIC(mvtype))
			*ret = literal_zero;
		else
		{
			MV_FORCE_DEFINED(u);
			/* If we reach here, it means undef_inhibit is TRUE and "u" was undefined.
			 * Treat it as "" in that case. Therefore return TRUE (i.e. u="" is TRUE).
			 */
			assert(undef_inhibit);	/* else above would have issued a LVUNDEF error */
			*ret = literal_one;
		}
	} else if (MVTYPE_IS_SQLNULL(mvtype))
		*ret = literal_sqlnull;
	else
		*ret = (u->str.len ? literal_zero : literal_one);
	return;
}
