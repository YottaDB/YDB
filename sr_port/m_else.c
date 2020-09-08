/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "mmemory.h"
#include "cmd.h"

error_def(ERR_SPOREOL);

int m_else(void)
{
	triple	*jmpref, elsepos_in_chain, *gettruth, *cobool;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	elsepos_in_chain = TREF(pos_in_chain);
	if (TK_EOL == TREF(window_token))
		return TRUE;
	if (TK_SPACE != TREF(window_token))
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	gettruth = newtriple(OC_GETTRUTH);
	cobool = newtriple(OC_COBOOL);
	cobool->operand[0] = put_tref(gettruth);
	// NOTE: this passes in a dummy value for JMP_DEPTH,
	// because it will be ignored when the 2nd macro parameter is INIT_GBL_BOOL_DEPTH.
	ADD_BOOL_ZYSQLNULL_PARMS(cobool, INIT_GBL_BOOL_DEPTH, OC_NOOP, OC_NOOP,
					CALLER_IS_BOOL_EXPR_FALSE, IS_LAST_BOOL_OPERAND_FALSE, 0);
	jmpref = newtriple(OC_JMPNEQ);
	FOR_END_OF_SCOPE(0, jmpref->operand[0]);
	if (!linetail())
	{	tnxtarg(&jmpref->operand[0]);
		TREF(pos_in_chain) = elsepos_in_chain;
		return FALSE;
	} else
		return TRUE;
}
