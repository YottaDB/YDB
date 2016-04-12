/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "op.h"
#include "lv_val.h"
#include "mvalconv.h"
#include "glvn_pool.h"

error_def(ERR_ORDER2);
error_def(ERR_VAREXPECTED);

void	op_indo2(mval *dst, uint4 indx, mval *direct)
{
	glvn_pool_entry	*slot;
	int4		dummy_intval;
	intszofptr_t	n;
	lv_val		*lv;
	mval		*key;
	opctype		oc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_NUM(direct);
	if (!MV_IS_TRUEINT(direct, &dummy_intval) || (direct->m[1] != (1 * MV_BIAS) && direct->m[1] != (-1 * MV_BIAS)))
		rts_error(VARLSTCNT(1) ERR_ORDER2);
	slot = &((TREF(glvn_pool_ptr))->slot[indx]);
	oc = slot->sav_opcode;
	if (OC_SAVLVN == oc)
	{	/* lvn */
		n = --slot->glvn_info.n;
		if (0 == n)
		{	/* lvn name */
			slot->glvn_info.n++;				/* quick restore count so glvnpop works correctly */
			/* like op_fnlvnameo2 */
			if ((1 * MV_BIAS) == direct->m[1])
				op_fnlvname(slot->lvname, FALSE, dst);
			else
				op_fnlvprvname(slot->lvname, dst);
		} else
		{	/* subscripted lv */
			key = (mval *)slot->glvn_info.arg[n];
			lv = op_rfrshlvn(indx, OC_RFRSHLVN);		/* funky opcode prevents UNDEF in rfrlvn */
			slot->glvn_info.n++;				/* quick restore count so glvnpop works correctly */
			/* like op_fnno2 */
			if ((1 * MV_BIAS) == direct->m[1])
				op_fnorder(lv, key, dst);
			else
				op_fnzprevious(lv, key, dst);
		}
	} else if (OC_NOOP != oc)					/* if indirect error blew set up, skip this */
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		/* like op_gvno2 */
		if ((1 * MV_BIAS) == direct->m[1])
			op_gvorder(dst);
		else
			op_zprevious(dst);
	}
	return;
}
