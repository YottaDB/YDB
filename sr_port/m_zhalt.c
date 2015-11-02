/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cmd.h"
#include "compiler.h"
#include "indir_enum.h"
#include "opcode.h"
#include "toktyp.h"

GBLREF char	window_token;

LITREF mval	literal_zero;

/* Halt the process similar to op_halt but allow a return code to be specified. If no return code
 * is specified, return code 0 is used as a default (making it identical to op_halt).
 */
int m_zhalt(void)
{
	triple	*triptr;
	oprtype ot;
	int	status;

	/* Let m_halt() handle the case of the missing return code */
	if (window_token == TK_SPACE || window_token == TK_EOL)
		return m_halt();
	switch (status = numexpr(&ot))		/* note assignment */
	{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			triptr = newtriple(OC_ZHALT);
			triptr->operand[0] = ot;
			return TRUE;
		case EXPR_INDR:
			make_commarg(&ot, indir_zhalt);
			return TRUE;
		default:
			GTMASSERT;		/* At least in debug builds we will see the status */
	}
	return FALSE; /* This should never get executed, added to make compiler happy */
}
