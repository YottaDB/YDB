/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mvalconv.h"
#include "op.h"
#include "is_canonic_name.h"

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
	error_def(ERR_NOCANONICNAME);

	if (!is_canonic_name(src, &subscripts, &dummy1, &dummy2))
		rts_error(VARLSTCNT(4) ERR_NOCANONICNAME, 2, src->str.len, src->str.addr);
        MV_FORCE_MVAL(dst, subscripts);		/* is_canonic_name has to parse anyway, so take count from it */
	return;
}
