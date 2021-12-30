/****************************************************************
 *								*
 * Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"

/* The purpose of this condition handler is to catch any runtime errors (e.g. DIVZERO etc.)
 * inside "ex_arithlit_compute()" and return control to the caller of that function ("ex_arithlit()").
 * (this lets that caller function know that binary arithmetic operation literal optimization is not
 * possible at compile time). Hence the UNWIND macro usage below.
 */
CONDITION_HANDLER(ex_arithlit_compute_ch)
{
	START_CH(TRUE);
	if (DUMP)
	{
		NEXTCH;
	}
	UNWIND(NULL, NULL);
}
