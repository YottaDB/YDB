/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mvalconv.h"
#include "stringpool.h"
#include "op.h"
#include "is_canonic_name.h"
#include "zshow.h"

error_def(ERR_NOCANONICNAME);

/*
 * -----------------------------------------------
 * op_fnqlength()
 * MUMPS QLength function
 *
 * Arguments:
 *	src	- Pointer to Source Name string mval
 *	dst	- destination buffer to save the piece in
 * Return:
 *	none
 * -----------------------------------------------
 */
void op_fnqlength(mval *src, mval *dst)
{
	int	dummy1;
	int	dummy2;
	int	subscripts = -2; /* no interest in finding a particular component */

	if (!is_canonic_name(src, &subscripts, &dummy1, &dummy2))
		NOCANONICNAME_ERROR(src);
        MV_FORCE_MVAL(dst, subscripts);		/* is_canonic_name has to parse anyway, so take count from it */
	return;
}
