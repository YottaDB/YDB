/****************************************************************
 *								*
 *	Copyright 2011, 2012 Fidelity Information Services, Inc 	*
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
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"
#include "error.h"

error_def(ERR_INVCMD);

/* Consume and ignore the arguments, and insert an INVCMD error triple.
 * Previously, we raised an error immediately when the invalid command was
 * detected, but since a false postconditional may skip the error and
 * continue executing the line, we need to parse it properly.
 */

int m_zinvcmd(void)
{
	triple *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	while ((TK_EOL != TREF(window_token)) && (TK_SPACE != TREF(window_token)) && (TK_ERROR != TREF(window_token)))
		advancewindow();
	if (TK_ERROR == TREF(window_token))
		return FALSE;
	triptr = newtriple(OC_RTERROR);
	triptr->operand[0] = put_ilit(MAKE_MSG_TYPE(ERR_INVCMD, ERROR));	/* switch from warning to error */
	triptr->operand[1] = put_ilit(FALSE);					/* not a subroutine reference */
	return TRUE;
}
