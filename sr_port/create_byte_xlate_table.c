/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_string.h"
#include "mdef.h"
#include "compiler.h"
#include "toktyp.h"
#include "op.h"

/**
 * Generates a translate table where each byte in the search string (srch) is mapped to the corresponding byte in the replace
 *   string (repl).
 *
 * The result translate table contains the replace character at each search character's ASCII value; that is, if srch = "A",
 *  rplc="B", xlate['A' = 97] = 'B';
 * For characters that aren't to be replaced, NO_VALUE is set. For characters that are to be deleted, DELETE_VALUE is set.
 *
 * @param [in] srch string representing the search characters
 * @param [in] rplc string representing the replace characters
 * @param [out] the allocate translate table; expected to be unitt'ed memory of size SIZEOF(int4) * NUM_CHARS
 */
void create_byte_xlate_table(mval *srch, mval *rplc, int4 *xlate)
{
	sm_uc_ptr_t stop, scur, rtop, rcur;

	memset(xlate, NO_VALUE, SIZEOF(int4) * NUM_CHARS);
	scur = (sm_uc_ptr_t)srch->str.addr;
	stop = scur + srch->str.len;
	rcur = (sm_uc_ptr_t)rplc->str.addr;
	rtop = rcur + rplc->str.len;

	for (; (scur < stop) && (rcur < rtop); scur++, rcur++ )
		if (NO_VALUE == xlate[*scur])
			xlate[*scur] = *rcur;
	for (; scur < stop; scur++)
		if (NO_VALUE == xlate[*scur])
			xlate[*scur] = DELETE_VALUE;
}
