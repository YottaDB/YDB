/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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

error_def(ERR_FNNAMENEG);
error_def(ERR_NOCANONICNAME);

/*
 * Removes extra subscripts from result of op_indfnname. Similar to $QLENGTH and $QSUBSCRIPT.
 * E.g. $name(@"x(6,7,8)",2):
 * 1. Do op_indfnname and find ALL subscripts --> "x(6,7,8)"
 * 2. Evaluate second argument --> 2
 * 3. Using the results of the previous two steps, do op_indfnname2 to remove extra subscripts --> "x(6,7)"
 */
void op_indfnname2(mval *finaldst, mval *depthval, mval *prechomp)
{
	int	depth, dummy, start, subscripts;
	char	*c;

	MV_FORCE_STR(prechomp);
	depth = MV_FORCE_INT(depthval);
	if (depth < 0)
		rts_error(VARLSTCNT(1) ERR_FNNAMENEG);
	subscripts = depth + 1;
	*finaldst = *prechomp;
	if (subscripts > MAX_LVSUBSCRIPTS)
		return;
	if (!is_canonic_name(prechomp, &subscripts, &start, &dummy))
	{	/* op_indfnname should have passed us a valid name */
		assert(FALSE);
		rts_error(VARLSTCNT(4) ERR_NOCANONICNAME, 2, prechomp->str.len, prechomp->str.addr);
	}
	if (start)
	{	/* indeed have subscripts to remove */
		c = finaldst->str.addr + start - 1;
		if ('"' == *c)
		{
			c--;
			start--;
		}
		assert(('(' == *c) || (',' == *c));
		if ('(' == *c)
			start--;
		else
			*c = ')';
		finaldst->str.len = start;
	}
	return;
}
