/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "op.h"

#ifdef DEBUG
GBLREF	bool	undef_inhibit;
#endif

/* Note: This C function is called by "opp_equnul_retbool.s" on sr_aarch64 and sr_armv7l.
 * On sr_x86_64, the .s file implements this functionality for performance reasons.
 */
int	op_equnul_retbool(mval *u)
{
	int	mvtype;
	int	ret;

	mvtype = u->mvtype;
	if (!MVTYPE_IS_STRING(mvtype))
	{
		if (MVTYPE_IS_NUMERIC(mvtype))
			ret = FALSE;
		else
		{
			MV_FORCE_DEFINED(u);
			/* If we reach here, it means undef_inhibit is TRUE and "u" was undefined.
			 * Treat it as "" in that case. Therefore return TRUE (i.e. u="" is TRUE).
			 */
			assert(undef_inhibit);	/* else above would have issued a LVUNDEF error */
			ret = TRUE;
		}
	} else if (MVTYPE_IS_SQLNULL(mvtype))
		ret = FALSE;
	else
		ret = (u->str.len ? FALSE : TRUE);
	return ret;
}
